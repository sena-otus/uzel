#include "netapp.h"
#include <stdexcept>
#include <uzel/session.h>
#include <uzel/uconfig.h>

using boost::asio::ip::tcp;

NetClient::NetClient(boost::asio::io_context& io_context, unsigned short port)
  : m_aresolver{5, io_context}, m_work{boost::ref(io_context)}//, m_iocontext(io_context)
{
}

void NetClient::start()
{
  auto self(shared_from_this());
  std::cout << "try to resolve localhost.." << std::endl;
  m_aresolver.async_resolve<tcp>("localhost", "32300",
                                 [this, self](const boost::system::error_code ec, const tcp::resolver::results_type resit){
                                   self->connectResolved(ec,resit);
                                 });
}


void NetClient::connectResolved(const boost::system::error_code ec, const tcp::resolver::results_type rezit)
{
  if(ec) {
    std::cout  << "error resolving: "  <<  ec.message() << "\n";
  } else {
    std::cout << "connecting to  "  << rezit->host_name() << "->" << rezit->endpoint() << "...\n";
    tcp::socket sock{m_work->get_io_context()};
    auto unauth = std::make_shared<session>(std::move(sock));
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
    throw std::runtime_error("should not get remote connection here");
  }
}
