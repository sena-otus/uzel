#include "netserver.h"
#include <uzel/session.h>
#include <uzel/uconfig.h>

using boost::asio::ip::tcp;
namespace io = boost::asio;
namespace sys = boost::system;

const int delay_reconnect_s = 10;
const int resolver_threads = 1;


NetServer::NetServer(io::io_context& io_context, unsigned short port)
  : m_acceptor(io_context, tcp::endpoint(tcp::v6(), port)),  m_aresolver{5, io_context}, m_iocontext(io_context)
{
    // that throws exception!
    // TODO:  check how to properly set an option
    // m_acceptor.set_option(boost::asio::ip::v6_only(false));

  do_accept();
  auto to_connect_to = uzel::UConfigS::getUConfig().remotes();
  for(auto && rhost : to_connect_to)
  {
    if(uzel::UConfigS::getUConfig().isLocalNode(rhost)) continue;
    startResolving(rhost);
  }
}


void NetServer::reconnectAfterDelay(const std::string &hname)
{
  auto timer = std::make_shared<io::steady_timer>(m_iocontext, io::chrono::seconds(delay_reconnect_s));
  timer->async_wait([&,timer, hname](const sys::error_code&  /*ec*/){ startResolving(hname);});
}


void NetServer::startResolving(const std::string &hname)
{
  m_aresolver.async_resolve<tcp>(hname, "32300",
                                 [this](const sys::error_code ec, const tcp::resolver::results_type resit){
                                   connectResolved(ec,resit);
                                 });
}


void NetServer::connectResolved(const sys::error_code ec, const tcp::resolver::results_type rezit)
{
  if(ec) {
    std::cout  << "error resolving: "  <<  ec.message() << "\n";
  } else {
    std::cout << "connecting to  "  << rezit->host_name() << "->" << rezit->endpoint() << "...\n";
    tcp::socket sock{m_iocontext};
    auto unauth = std::make_shared<session>(std::move(sock));
    unauth->s_connect_error.connect([&](const std::string &hname){ reconnectAfterDelay(hname);});
    unauth->s_auth.connect([&](session::shr_t ss){ auth(ss); });
    unauth->s_dispatch.connect([&](uzel::Msg &msg){ dispatch(msg);});
    unauth->startConnection(rezit);
  }
}


void NetServer::do_accept()
{
  m_acceptor.async_accept(
    [this](sys::error_code ec, tcp::socket socket)
      {
        if (!ec) {
          auto unauth = std::make_shared<session>(std::move(socket));
          unauth->s_auth.connect([&](session::shr_t ss){ auth(ss); });
          unauth->s_dispatch.connect([&](uzel::Msg &msg){ dispatch(msg);});
          unauth->start();
        }

        do_accept();
      });
}



void NetServer::localMsg(uzel::Msg & msg)
{
  auto lit = m_locals.find(msg.dest().app());
  if(lit != m_locals.end()) {
    lit->second->putOutQueue(std::move(msg));
  }
}

void NetServer::localbroadcastMsg(uzel::Msg & msg)
{
  std::for_each(m_locals.begin(), m_locals.end(),
                [&msg](auto &sp){
                  sp.second->putOutQueue(msg);
                });

}


void NetServer::broadcastMsg(uzel::Msg & msg)
{
  std::for_each(m_remotes.begin(), m_remotes.end(),
                [&msg](auto &sp){
                  sp.second->putOutQueue(msg);
                });
}


void NetServer::remoteMsg(uzel::Msg & msg)
{
  auto rit = m_remotes.find(msg.dest().node());
  if(rit != m_remotes.end())
  {
    rit->second->putOutQueue(std::move(msg));
  }
}


void NetServer::serviceMsg(uzel::Msg &msg [[maybe_unused]])
{
  return;
}

void NetServer::dispatch(uzel::Msg &msg)
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

void NetServer::auth(session::shr_t ss)
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
