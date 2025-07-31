#include "netclient.h"
#include "session.h"
#include "msg.h"
#include "dbg.h"

//#include "uconfig.h"

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/log/trivial.hpp>
#include <boost/ref.hpp> // NOLINT(misc-include-cleaner)
#include <boost/chrono.hpp> // NOLINT(misc-include-cleaner)
#include <boost/system/error_code.hpp>
#include <memory>
#include <iostream>
#include <string>
#include <utility>

using boost::asio::ip::tcp;
namespace io = boost::asio;

const int delay_reconnect_s = 10;
const int resolver_threads = 1;

NetClient::NetClient(io::io_context& io_context, unsigned short port)
  : m_aresolver{resolver_threads, io_context}, m_work{boost::ref(io_context)},  // NOLINT(misc-include-cleaner)
    m_reconnectTimer(io_context, io::chrono::seconds(delay_reconnect_s)), // NOLINT(misc-include-cleaner)
    m_port(port)
{
  start();
}

void NetClient::start()
{
  BOOST_LOG_TRIVIAL(debug) << DBGOUT << "resolve localhost...";
  m_aresolver.async_resolve<tcp>("localhost", std::to_string(m_port),
                                 [this](const boost::system::error_code ec, const tcp::resolver::results_type resit){
                                   connectResolved(ec,resit,"localhost");
                                 });
}


void NetClient::reconnectAfterDelay()
{
  m_reconnectTimer.expires_after(io::chrono::seconds(delay_reconnect_s));
  m_reconnectTimer.async_wait([this](const boost::system::error_code&  /*ec*/){start();});
}

void NetClient::connectResolved(const boost::system::error_code ec, const tcp::resolver::results_type &rezit, const std::string &hname)
{
  if(ec) {
    BOOST_LOG_TRIVIAL(error)  << "error resolving '" << hname  << "' :" <<  ec.message();
      // sleep 10 seconds and try to resolve again
    reconnectAfterDelay();
  } else {
    BOOST_LOG_TRIVIAL(info) << "connecting to  "  << rezit->host_name() << "->" << rezit->endpoint();
    tcp::socket sock{m_work->get_io_context()};
    auto unauth = std::make_shared<uzel::session>(std::move(sock), uzel::Direction::outgoing, rezit->endpoint().address(), rezit->host_name());
    unauth->s_connect_error.connect([&](const std::string &){ reconnectAfterDelay();});
    unauth->s_auth.connect([&](uzel::session::shr_t ss){ auth(ss); });
    unauth->s_dispatch.connect([&](uzel::Msg::shr_t msg){ dispatch(msg);});
    unauth->startConnection(rezit);
  }
}


void NetClient::dispatch(uzel::Msg::shr_t msg)
{
}

void NetClient::auth(uzel::session::shr_t ss)
{
  if(ss->msg1().fromLocal()) {
    auto appname = ss->msg1().from().app();
    BOOST_LOG_TRIVIAL(info) << "store local session with name " << appname;
    auto oldSessionIt = m_locals.find(appname);
    if(oldSessionIt != m_locals.end()) {
        // found old session: move old messages from it to the new session
      ss->takeOverMessages(*(oldSessionIt->second));
    }
    m_locals[appname] = ss;
    s_authSuccess();
  } else {
    BOOST_LOG_TRIVIAL(error) << "connected to something wrong, close connnection and reconnect after delay...";
      // throw std::runtime_error("should not get remote connection here");
    ss->gracefullClose("bad auth");
    reconnectAfterDelay();
  }
}


void NetClient::send(uzel::Msg::shr_t msg)
{
  m_locals["userver"]->putOutQueue(msg);
}
