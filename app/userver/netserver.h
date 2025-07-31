#pragma once

#include "remote.h"
#include "OutgoingManager.h"

#include <uzel/aresolver.h>
#include <uzel/session.h>

#include <boost/asio.hpp>
#include <map>
#include <unordered_map>


namespace uzel
{
  class session;
}


/** @brief tcp server */
class NetServer
{
public:
    /**
     * @brief server app ctor
     * @param io_context boost::asio io context
     * @param port TCP port to accept connections */
  NetServer(boost::asio::io_context& io_context, unsigned short port);


  void auth(uzel::session::shr_t ss);
  void dispatch(uzel::Msg::shr_t msg);
private:
  void do_accept();
  void serviceMsg(uzel::Msg::shr_t msg);
  void localMsg(uzel::Msg::shr_t msg);
  void remoteMsg(uzel::Msg::shr_t msg);
  void broadcastMsg(uzel::Msg::shr_t msg);
  void localbroadcastMsg(uzel::Msg::shr_t msg);
  void connectResolved( boost::system::error_code ec,  boost::asio::ip::tcp::resolver::results_type rezit, const std::string &hname);
  void reconnectAfterDelay(const std::string &hname);
//  void startResolving(const std::string &hname);
//  void startConnecting(const std::string &hname);
//  void startConnecting(std::shared_ptr<boost::asio::steady_timer> timer);
    /**
     *  find channel to remote node, if it does not exist, then create a new one
     *  */
  uzel::remote &findAddRemote(const std::string &node);
  std::optional<std::string> route(const std::string &target) const;

    /** add authenticated session to remote channel */
  void addAuthSessionToRemote(const std::string &rnode, uzel::session::shr_t ss);
  void onSessionClosed(uzel::session::shr_t ss);
  void onSessionCreated(uzel::session::shr_t newSession);

  const int MaxConnectionsWithAddr = 10;
  const unsigned ResolverThreads = 5;

  uzel::IpToSession m_ipToSession;
  boost::asio::ip::tcp::acceptor m_acceptor;
  std::map<std::string, uzel::session::shr_t> m_locals;
  uzel::NodeToSession m_nodeToSession; ///<! map nodes to sessions

  aresolver m_aresolver;
  boost::asio::io_context& m_iocontext;
  OutgoingManager m_outman;
};
