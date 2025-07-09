#include "netserver.h"
#include "uzel/msg.h"
#include <boost/log/trivial.hpp>
#include <uzel/session.h>
#include <uzel/uconfig.h>
#include <uzel/dbg.h>

#include <algorithm>

using boost::asio::ip::tcp;
using std::for_each;
namespace io = boost::asio;
namespace sys = boost::system;

const int delay_reconnect_s = 10;
const int resolver_threads = 5;


remote::remote(std::string nodename)
  : m_node(std::move(nodename))
{
}


void remote::addSession(session::shr_t ss)
{
  bool wasEmpty = m_session.empty();
  m_session.push_back(ss);
  BOOST_LOG_TRIVIAL(debug) << "there are now " << m_session.size() << " session(s) for " << m_node;
  ss->s_send_error.connect([&]() { on_session_error(ss);});
  ss->s_receive_error.connect([&]() { on_session_error(ss);});
  if(wasEmpty)
  { // first session
    ss->takeOverMessages(m_outHighQueue);
  } else {
    // secondary connection or new connection
    // * secondary connection will have the same remote UUID
    // * new connection will have new remote UUID, that could be
    // ** remote app was restarted
    // ** remote app is a duplicate
  }
}

bool remote::connected() const
{
  return !m_session.empty();
}

void remote::on_session_error(session::shr_t ss)
{
  BOOST_LOG_TRIVIAL(debug) << "error on session " << ss << ", excluding it from list";
  for(auto ssi = m_session.begin(); ssi != m_session.end(); ++ssi)
  {
    if(*ssi == ss)
    {
      m_session.erase(ssi);
      BOOST_LOG_TRIVIAL(debug) << "excluded, new list size is " << m_session.size();
      return;
    }
  }
}


void remote::send(const uzel::Msg &msg)
{
  if(m_session.empty())
  {
      // no connection
    m_outHighQueue.emplace(msg.str());
  }
  else {
      // the very last session is the highest priority!
    m_session.back()->putOutQueue(msg);
  }
}



NetServer::NetServer(io::io_context& io_context, unsigned short port)
  : m_acceptor(io_context, tcp::endpoint(tcp::v6(), port)),  m_aresolver{resolver_threads, io_context}, m_iocontext(io_context)
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
  BOOST_LOG_TRIVIAL(debug) << DBGOUT << " resolving " << hname << "...";
  m_aresolver.async_resolve<tcp>(hname, "32300",
                                 [this,hname](const sys::error_code ec, const tcp::resolver::results_type resit){
                                   BOOST_LOG_TRIVIAL(debug) << DBGOUT;
                                   connectResolved(ec,resit,hname);
                                 });
}


void NetServer::connectResolved(const sys::error_code ec, const tcp::resolver::results_type rezit, const std::string &hname)
{
  if(ec) {
    BOOST_LOG_TRIVIAL(error) << "error resolving '" << hname << "': "<<  ec.message();
    reconnectAfterDelay(hname);
  } else {
    BOOST_LOG_TRIVIAL(info) << "resolved "  << rezit->host_name() << "->" << rezit->endpoint() << ", connecting...";
    tcp::socket sock{m_iocontext};
    auto unauth = std::make_shared<session>(std::move(sock));
    unauth->s_connect_error.connect([&](const std::string &hname){ reconnectAfterDelay(hname);});
    unauth->s_auth.connect([&](session::shr_t ss){ auth(ss); });
    unauth->s_dispatch.connect([&](uzel::Msg &msg){ dispatch(msg);});
    unauth->s_send_error.connect([&](){ on_session_error(unauth);});
    unauth->s_receive_error.connect([&](){on_session_error(unauth); });
    unauth->startConnection(rezit);
  }
}


void NetServer::on_session_error(session::shr_t ss)
{
    // ss->disconnect(); // disconnect is called in ession itself
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
  std::ranges::for_each(m_locals,
                        [&msg](auto &sp){
                          sp.second->putOutQueue(msg);
                        });

}


void NetServer::broadcastMsg(uzel::Msg &msg)
{
  std::ranges::for_each(m_remotes,
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


remote &NetServer::findAddRemote(const std::string &node)
{
  auto remoteIt = m_remotes.find(node);
  if(remoteIt == m_remotes.end())
  {
    BOOST_LOG_TRIVIAL(info) << "created new remote channel for node " << node;
    remoteIt = m_remotes.insert({node, remote(node)}).first;
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


void NetServer::addAuthSessionToRemote(const std::string &rnode, session::shr_t ss)
{
  auto &remote = findAddRemote(rnode);
  remote.addSession(ss);
}

void NetServer::auth(session::shr_t ss)
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
