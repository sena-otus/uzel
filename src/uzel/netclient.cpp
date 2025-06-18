#include "netclient.h"
#include "session.h"
#include "msg.h"

//#include "uconfig.h"

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/io_context.hpp>
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
  std::cout << "resolve localhost...\n";
  m_aresolver.async_resolve<tcp>("localhost", std::to_string(m_port),
                                 [this](const boost::system::error_code ec, const tcp::resolver::results_type &resit){
                                   connectResolved(ec,resit);
                                 });
}


void NetClient::reconnectAfterDelay()
{
  m_reconnectTimer.expires_after(io::chrono::seconds(delay_reconnect_s));
  m_reconnectTimer.async_wait([this](const boost::system::error_code&  /*ec*/){start();});
}

void NetClient::connectResolved(const boost::system::error_code ec, const tcp::resolver::results_type &rezit)
{
  if(ec) {
    std::cout  << "error resolving: "  <<  ec.message() << "\n";
      // sleep 10 seconds and try to resolve again
    reconnectAfterDelay();
  } else {
    std::cout << "connecting to  "  << rezit->host_name() << "->" << rezit->endpoint() << "...\n";
    tcp::socket sock{m_work->get_io_context()};
    auto unauth = std::make_shared<session>(std::move(sock));
    unauth->s_connect_error.connect([&](const std::string &){ reconnectAfterDelay();});
    unauth->s_auth.connect([&](session::shr_t ss){ auth(ss); }); // NOLINT(performance-unnecessary-value-param)
    unauth->s_dispatch.connect([&](uzel::Msg &msg){ dispatch(msg);});
    unauth->startConnection(rezit);
  }
}


void NetClient::dispatch(uzel::Msg &msg)
{
}

void NetClient::auth(session::shr_t ss)
{
  if(ss->msg1().fromLocal()) {
    std::cout << "store local session with name " << ss->msg1().from().app() << "\n";
    m_locals.emplace(ss->msg1().from().app(), ss);
    s_authSuccess();
  } else {
    std::cerr << "connected to something wrong, close connnection and reconnect after delay...\n" << std::flush ;
      // throw std::runtime_error("should not get remote connection here");
    ss->disconnect();
    reconnectAfterDelay();
  }
}


void NetClient::send(uzel::Msg &&msg)
{
  m_locals["userver"]->putOutQueue(std::move(msg));
}
