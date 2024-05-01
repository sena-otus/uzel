#include "netclient.h"
#include <stdexcept>
#include <uzel/session.h>
#include <uzel/uconfig.h>

using boost::asio::ip::tcp;
namespace io = boost::asio;

const int delay_reconnect_s = 10;
const int resolver_threads = 1;

NetClient::NetClient(io::io_context& io_context, unsigned short port)
  : m_aresolver{resolver_threads, io_context}, m_work{boost::ref(io_context)},
    m_reconnect_timer(io_context, io::chrono::seconds(delay_reconnect_s)),
    m_port(port)
{
  start();
}

void NetClient::start()
{
  std::cout << "resolve localhost...\n";
  m_aresolver.async_resolve<tcp>("localhost", std::to_string(m_port),
                                 [this](const boost::system::error_code ec, const tcp::resolver::results_type resit){
                                   connectResolved(ec,resit);
                                 });
}


void NetClient::reconnectAfterDelay()
{
  m_reconnect_timer.async_wait([this](const boost::system::error_code&  /*ec*/){start();});
}

void NetClient::connectResolved(const boost::system::error_code ec, const tcp::resolver::results_type rezit)
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
    unauth->s_auth.connect([&](session::shr_t ss){ auth(ss); });
    unauth->s_dispatch.connect([&](uzel::Msg &msg){ dispatch(msg);});
    unauth->startConnection(rezit);
  }
}


void NetClient::dispatch(uzel::Msg &msg)
{
  std::cout << "Got message: " << msg.str();
}

void NetClient::auth(session::shr_t ss)
{
  if(ss->msg1().fromLocal()) {
    std::cout << "store local session with name " << ss->msg1().from().app() << "\n";
    m_locals.emplace(ss->msg1().from().app(), ss);
  } else {
    std::cerr << "connected to something wrong, failing..." << std::endl;
      // throw std::runtime_error("should not get remote connection here");
    ss->disconnect();
    reconnectAfterDelay();
  }
}
