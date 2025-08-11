#include "netserver.h"

#include "uzel/msg.h"
#include "uzel/netappbase.h"
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
  : m_netctx(std::make_shared<uzel::NetAppContext>(io_context)), m_acceptor(io_context, tcp::endpoint(tcp::v6(), port)),
    m_outman(m_netctx, m_ipToSession, m_nodeToSession)
{
    // that throws exception!
    // TODO:  check how to properly set an option
    // m_acceptor.set_option(boost::asio::ip::v6_only(false));
  do_accept();
  auto to_connect_to = uzel::UConfigS::getUConfig().remotes();
  for(auto && rhost : to_connect_to)
  {
    m_outman.startConnecting(rhost);
  }

  m_outman.s_sessionCreated.connect([&](uzel::session::shr_t ss) {
      onSessionCreated(ss);
  });
  m_outman.startConnecting();
}


  // void NetServer::dispatch(Msg::shr_t msg, session::shr_t ss)
  // {
  //   if(msg->toMe()) {
  //     m_dispatcher.dispatch(*msg, ss);
  //   }

  //   switch(msg->destType())
  //   {
  //     case Msg::DestType::service: {
  //       handleServiceMsg(msg, ss);
  //       break;
  //     }
  //     case Msg::DestType::local: {
  //       handleLocalMsg(msg);
  //       break;
  //     }
  //     case Msg::DestType::remote: {
  //       handleRemoteMsg(msg);
  //       break;
  //     }
  //     case Msg::DestType::localbroadcast: {
  //       handleLocalBroadcastMsg(msg);
  //       break;
  //     }
  //     case Msg::DestType::broadcast: {
  //       handleBroadcastMsg(msg);
  //       break;
  //     }
  //   }
  // }



void NetServer::onSessionCreated(uzel::session::shr_t newSession)
{
  m_ipToSession[newSession->remoteIp()].insert(newSession);

  newSession->s_closed.connect([&](uzel::session::shr_t ss){ onSessionClosed(ss);});
  newSession->s_auth.connect([&](uzel::session::shr_t ss){ auth(ss); });
//  newSession->s_dispatch.connect([&](uzel::Msg::shr_t msg, uzel::session::shr_t ss){ dispatch(msg,ss);});
}



void NetServer::do_accept()
{
  m_acceptor.async_accept(
    [this](sys::error_code ec, tcp::socket socket)
      {
        if (!ec) {
          auto raddr = socket.remote_endpoint().address();
          auto unauth = std::make_shared<uzel::session>(m_netctx, std::move(socket), uzel::Direction::incoming, raddr);
          onSessionCreated(unauth);
          if(m_ipToSession[raddr].size() > MaxConnectionsWithAddr) {
            unauth->gracefullClose("connection refused: too many connections");
            BOOST_LOG_TRIVIAL(warning) << "refused connection from "  << raddr << ", too many connections...";
          } else {
            unauth->start();
          }
        }
        do_accept();
      });
}


void NetServer::onSessionClosed(uzel::session::shr_t ss)
{
  m_ipToSession[ss->remoteIp()].erase(ss);
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
  auto remoteIt = m_nodeToSession.find(node);
  if(remoteIt == m_nodeToSession.end())
  {
    BOOST_LOG_TRIVIAL(info) << "created new remote channel for node " << node;
    remoteIt = m_nodeToSession.insert({node, uzel::remote(m_netctx, node)}).first;
  } else {
    BOOST_LOG_TRIVIAL(info) << "found existing remote channel for node " << node;
  }
  return remoteIt->second;
}


void NetServer::handleLocalMsg(uzel::Msg::shr_t msg)
{
  auto lit = m_locals.find(msg->dest().app());
  if(lit != m_locals.end()) {
    lit->second->putOutQueue(msg);
  }
}

void NetServer::handleLocalBroadcastMsg(uzel::Msg::shr_t msg)
{
  std::ranges::for_each(m_locals,
                        [&msg](auto &sp){
                          sp.second->putOutQueue(msg);
                        });

}


void NetServer::handleBroadcastMsg(uzel::Msg::shr_t msg)
{
  std::ranges::for_each(m_nodeToSession,
           [&msg](auto &sp) { sp.second.send(msg); });
}


void NetServer::handleRemoteMsg(uzel::Msg::shr_t msg)
{
    // first check if there are
    // special routing rules for that node

  std::string target = msg->dest().node();
  auto viaNode = route(target);

  if(!viaNode)
  {
    BOOST_LOG_TRIVIAL(error) << "Can not find route to '" << target << "', message is dropped";
    return;
  }

  auto &remote = findAddRemote(*viaNode);
  remote.send(msg);
}


void NetServer::addAuthSessionToRemote(const std::string &rnode, uzel::session::shr_t ss)
{
  auto &remote = findAddRemote(rnode);
  remote.addSession(ss);
}

void NetServer::auth(uzel::session::shr_t ss)
{
  if(ss->peerIsLocal()) {
    BOOST_LOG_TRIVIAL(debug) << DBGOUTF << "new local connection, store local session with name " << ss->peerApp();
      // if local app allows multiple instances, it should add pid to
      // the appname during auth, something like "usender:1251"
      // currently only single-instance applications are allowed
    auto ssIt = m_locals.find(ss->peerApp());
    if(ssIt != m_locals.end()) {
      ss->takeOverMessages(*(ssIt->second));
    }
    m_locals[ss->peerApp()] = ss;
  } else {
    auto rnode = ss->peerNode();
    addAuthSessionToRemote(rnode, ss);
  }
}
