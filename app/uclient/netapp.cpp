#include "netapp.h"
#include "session.h"
#include <uzel/uconfig.h>

using boost::asio::ip::tcp;

NetClient::NetClient(boost::asio::io_context& io_context, unsigned short port)
  : m_acceptor(io_context, tcp::endpoint(tcp::v6(), port)),  m_aresolver{5, io_context}, m_iocontext(io_context)
{
    // throws exception
    // TODO:  check how to properly set an option
    // m_acceptor.set_option(boost::asio::ip::v6_only(false));
  m_aresolver.async_resolve<tcp>("localhost", "32300",
                                 [this](const boost::system::error_code ec, const ip::tcp::resolver::results_type resit){
                                   connectResolved(ec,resit);
                                 });
  }
}

void NetClient::connectResolved(const boost::system::error_code ec, const boost::asio::ip::tcp::resolver::results_type rezit)
{
  if(ec) {
    std::cout  << "error resolving: "  <<  ec.message() << "\n";
  } else {
    std::cout << "connecting to  "  << rezit->host_name() << "->" << rezit->endpoint() << "...\n";
    boost::asio::ip::tcp::socket sock{m_iocontext};
    auto unauth = std::make_shared<session>(std::move(sock));
    unauth->s_auth.connect([&](session::shr_t ss){ auth(ss); });
    unauth->s_dispatch.connect([&](uzel::Msg &msg){ dispatch(msg);});
    unauth->startConnection(rezit);
  }
}

void NetClient::localMsg(uzel::Msg & msg)
{
  auto lit = m_locals.find(msg.dest().app());
  if(lit != m_locals.end()) {
    lit->second->putOutQueue(std::move(msg));
  }
}

void NetClient::localbroadcastMsg(uzel::Msg & msg)
{
  std::for_each(m_locals.begin(), m_locals.end(),
                [&msg](auto &sp){
                  sp.second->putOutQueue(msg);
                });

}


void NetClient::broadcastMsg(uzel::Msg & msg)
{
  std::for_each(m_remotes.begin(), m_remotes.end(),
                [&msg](auto &sp){
                  sp.second->putOutQueue(msg);
                });
}


void NetClient::remoteMsg(uzel::Msg & msg)
{
  auto rit = m_remotes.find(msg.dest().node());
  if(rit != m_remotes.end())
  {
    rit->second->putOutQueue(std::move(msg));
  }
}


void NetClient::serviceMsg(uzel::Msg &msg [[maybe_unused]])
{
  return;
}

void NetClient::dispatch(uzel::Msg &msg)
{
  switch(msg.destType())
  {
    case uzel::Msg::DestType::service:
      serviceMsg(msg);
      break;
    case uzel::Msg::DestType::local:
      localMsg(msg);
      break;
    case uzel::Msg::DestType::remote:
      remoteMsg(msg);
      break;
    case uzel::Msg::DestType::broadcast:
      broadcastMsg(msg);
      break;
    case uzel::Msg::DestType::localbroadcast:
      localbroadcastMsg(msg);
      break;
  }
}

void NetClient::auth(session::shr_t ss)
{
  if(ss->msg1().fromLocal()) {
    std::cout << "store local session with name " << ss->msg1().from().app() << "\n";
    m_locals.emplace(ss->msg1().from().app(), ss);
  } else {
    auto rname = ss->msg1().from().node();
    auto sit = m_remotes.find(rname);
    if(sit == m_remotes.end()) { // first connection
      m_remotes[rname] = ss;
    }
    else {
        // secondary connection or new connection
        // * secondary connection will have the same remote UUID
        // * new connection will have new remote UUID, that could be
        // ** remote app was restarted
        // ** remote app is a duplicate

        // store new connection as main and old one move to secondary
      m_remotes2[rname].emplace_front(std::move(sit->second));
      sit->second = ss;
        // TODO: cleanup old connections
    }
  }
}
