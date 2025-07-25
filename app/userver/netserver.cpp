#include "netserver.h"

#include "uzel/msg.h"
#include <uzel/session.h>
#include <uzel/uconfig.h>
#include <uzel/dbg.h>

#include <boost/log/trivial.hpp>
#include <algorithm>
//#include <functional>

using boost::asio::ip::tcp;
namespace io = boost::asio;
namespace sys = boost::system;




NetServer::NetServer(io::io_context& io_context, unsigned short port)
  : m_acceptor(io_context, tcp::endpoint(tcp::v6(), port)),  m_aresolver{ResolverThreads, io_context}, m_iocontext(io_context),
    m_conman(m_iocontext, m_aresolver, m_sessionByIp)
{
    // that throws exception!
    // TODO:  check how to properly set an option
    // m_acceptor.set_option(boost::asio::ip::v6_only(false));
  do_accept();
  auto to_connect_to = uzel::UConfigS::getUConfig().remotes();
  for(auto && rhost : to_connect_to)
  {
    m_conman.startConnecting(rhost);
  }

  m_conman.startConnecting();
}



// void NetServer::reconnectAfterDelay(const std::string &hname)
// {
//   auto timer = std::make_shared<io::steady_timer>(m_iocontext, io::chrono::seconds(DelayReconnect_sec));
//   timer->async_wait([&,timer, hname](const sys::error_code&  /*ec*/){ startResolving(hname);});
// }




// void NetServer::connectResolved(const sys::error_code ec, const tcp::resolver::results_type rezit, const std::string &hname)
// {
//   if(ec) {
//     BOOST_LOG_TRIVIAL(error) << "error resolving '" << hname << "': "<<  ec.message();
//     m_conman.setStatus(hname, HostStatus::resolving_error);
//   } else {
//     BOOST_LOG_TRIVIAL(info) << "resolved "  << rezit->host_name() << "->" << rezit->endpoint() << ", connecting...";
//     {
//       m_conman.setAddr(hname, rezit->endpoint().address());
//       m_conman.setStatus(hname, HostStatus::connecting);
//       tcp::socket sock{m_iocontext};
//       auto unauth = std::make_shared<uzel::session>(std::move(sock), uzel::Direction::outgoing, rezit->endpoint().address(), hname);
//       m_connectionsPerAddr[rezit->endpoint().address()].insert(unauth);
//       unauth->s_closed.connect([&](uzel::session::shr_t ss){ onSessionClosed(ss);});
//       unauth->s_connect_error.connect([&](const std::string &hname){ reconnectAfterDelay(hname);});
//       unauth->s_auth.connect([&](uzel::session::shr_t ss){ auth(ss); });
//       unauth->s_dispatch.connect([&](uzel::Msg &msg){ dispatch(msg);});
//       unauth->startConnection(rezit);
//     }
//   }
// }



void NetServer::do_accept()
{
  m_acceptor.async_accept(
    [this](sys::error_code ec, tcp::socket socket)
      {
        if (!ec) {
          auto unauth = std::make_shared<uzel::session>(std::move(socket), uzel::Direction::incoming, socket.remote_endpoint().address());
          m_sessionByIp[socket.remote_endpoint().address()].insert(unauth);
          unauth->s_closed.connect([&](uzel::session::shr_t ss){ onSessionClosed(ss);});
          if(m_sessionByIp[socket.remote_endpoint().address()].size() > MaxConnectionsWithAddr) {
            unauth->gracefullClose("connection refused: too many connections");
            BOOST_LOG_TRIVIAL(warning) << "refused connection from "  << socket.remote_endpoint() << ", to many connections...";
          } else {
            unauth->s_auth.connect([&](uzel::session::shr_t ss){ auth(ss); });
            unauth->s_dispatch.connect([&](uzel::Msg &msg){ dispatch(msg);});
            unauth->start();
          }
        }

        do_accept();
      });
}


void NetServer::onSessionClosed(uzel::session::shr_t ss)
{
  if(ss->direction() == +uzel::Direction::outgoing) {
      // we need to reconnect if
      // 1. remote hostname is listed in to_connect_to
      // and
      // 2. we do not have 2 active authorized connections to it already
  }
  m_sessionByIp[ss->remoteIp()].erase(ss);
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
  std::ranges::for_each(m_locals,
                        [&msg](auto &sp){
                          sp.second->putOutQueue(msg);
                        });

}


void NetServer::broadcastMsg(uzel::Msg &msg)
{
  std::ranges::for_each(m_node,
           [&msg](auto &sp) { sp.second.send(msg); });
}


std::optional<std::string> NetServer::route(const std::string & target) const
{
    // TODO: implenent routing
    //
    // return remote node name where we should send the message from that machine for the specified target
    // in case there are several alternatives, we will have to check which one is up

    // default is the original target - direct connection
  return target;
}


uzel::remote &NetServer::findAddRemote(const std::string &node)
{
  auto remoteIt = m_node.find(node);
  if(remoteIt == m_node.end())
  {
    BOOST_LOG_TRIVIAL(info) << "created new remote channel for node " << node;
    remoteIt = m_node.insert({node, uzel::remote(node)}).first;
  } else {
    BOOST_LOG_TRIVIAL(info) << "found existing remote channel for node " << node;
  }
  return remoteIt->second;
}



void NetServer::remoteMsg(const uzel::Msg &msg)
{
    // first check if there are
    // special routing rules for that node

  std::string target = msg.dest().node();
  auto viaNode = route(target);

  if(!viaNode)
  {
    BOOST_LOG_TRIVIAL(error) << "Can not find route to '" << target << "', message is dropped";
    return;
  }

  auto &remote = findAddRemote(*viaNode);
  remote.send(msg);
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


void NetServer::addAuthSessionToRemote(const std::string &rnode, uzel::session::shr_t ss)
{
  auto &remote = findAddRemote(rnode);
  remote.addSession(ss); // NOLINT(performance-unnecessary-value-param)
}

void NetServer::auth(uzel::session::shr_t ss) // NOLINT(performance-unnecessary-value-param)
{
  if(ss->msg1().fromLocal()) {
    BOOST_LOG_TRIVIAL(debug) << DBGOUTF << "new local connection, store local session with name " << ss->msg1().from().app();
      // if local app allows multiple instances, it should add pid to
      // the appname during auth, something like "usender:1251"
      // currently only single-instance applications are allowed
    auto ssIt = m_locals.find(ss->msg1().from().app());
    if(ssIt != m_locals.end()) {
      ss->takeOverMessages(*(ssIt->second));
    }
    m_locals[ss->msg1().from().app()] = ss;
  } else {
    auto rnode = ss->msg1().from().node();
    addAuthSessionToRemote(rnode, ss);
  }
}
