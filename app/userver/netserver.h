#pragma once

#include "remote.h"

#include <uzel/aresolver.h>
#include <uzel/session.h>
#include <boost/asio.hpp>
#include <map>

class session;


/** @brief tcp server */
class NetServer
{
public:
    /**
     * @brief server app ctor
     * @param io_context boost::asio io context
     * @param port TCP port to accept connections */
  NetServer(boost::asio::io_context& io_context, unsigned short port);


  void auth(session::shr_t ss);
  void dispatch(uzel::Msg &msg);
private:
  void do_accept();
  void serviceMsg(uzel::Msg &msg);
  void localMsg(uzel::Msg &msg);
  void remoteMsg(const uzel::Msg &msg);
  void broadcastMsg(uzel::Msg &msg);
  void localbroadcastMsg(uzel::Msg &msg);
  void connectResolved( boost::system::error_code ec,  boost::asio::ip::tcp::resolver::results_type rezit, const std::string &hname);
  void reconnectAfterDelay(const std::string &hname);
  void startResolving(const std::string &hname);
    /**
     *  find channel to remote node, if it does not exist, then create a new one
     *  */
  remote &findAddRemote(const std::string &node);
  std::optional<std::string> route(const std::string &target) const;

    /** add authenticated session to remote channel */
  void addAuthSessionToRemote(const std::string &rnode, session::shr_t ss);
  void on_session_error(session::shr_t ss);

  boost::asio::ip::tcp::acceptor m_acceptor;
  std::map<std::string, session::shr_t> m_locals;
  std::map<std::string, remote> m_node; //<! map nodes to channels

  aresolver m_aresolver;
  boost::asio::io_context& m_iocontext;
};
