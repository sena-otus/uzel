#pragma once

#include "remote.h"
#include "OutgoingManager.h"

#include <uzel/aresolver.h>
#include <uzel/session.h>
#include <uzel/dispatcher.h>
#include <uzel/netappcontext.h>

#include <boost/asio.hpp>
#include <map>


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
  NetServer() = delete;
  NetServer(const NetServer&) = delete;
  NetServer& operator=(const NetServer&) = delete;
  NetServer(NetServer&&) = delete;
  NetServer& operator=(NetServer&&) = delete;
  virtual ~NetServer() = default;

  void auth(uzel::session::shr_t ss);
private:
  void do_accept();
  void handleServiceMsg(uzel::Msg::shr_t msg, uzel::session::shr_t ss);
  void handleLocalMsg(uzel::Msg::shr_t msg);
  void handleRemoteMsg(uzel::Msg::shr_t msg);
  void handleBroadcastMsg(uzel::Msg::shr_t msg);
  void handleLocalBroadcastMsg(uzel::Msg::shr_t msg);

  void connectResolved( boost::system::error_code ec,  boost::asio::ip::tcp::resolver::results_type rezit, const std::string &hname);
  void reconnectAfterDelay(const std::string &hname);
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

  uzel::NetAppContext::shr_t m_netctx;
  uzel::IpToSession m_ipToSession;
  boost::asio::ip::tcp::acceptor m_acceptor;
  std::map<std::string, uzel::session::shr_t> m_locals;
  uzel::NodeToSession m_nodeToSession; ///<! map nodes to sessions
  uzel::OutgoingManager m_outman;
};
