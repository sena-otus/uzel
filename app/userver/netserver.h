#pragma once

#include <uzel/aresolver.h>
#include <uzel/session.h>
#include <boost/asio.hpp>
#include <list>
#include <map>

class session;

/** incapsulates logical channel to one remote */
class remote
{
public:
  explicit remote(std::string name);
  void addSession(session::shr_t);
  void send(const uzel::Msg &msg);
  [[nodiscard]] bool connected() const;
private:
  void on_session_error(session::shr_t ss);

  std::string m_node; ///! remote node name
  std::list<session::shr_t> m_session; ///! keeps list of tcp sessions
  std::queue<std::string> m_outHighQueue; ///! high prirority outgoing queue
  std::queue<std::string> m_outLowQueue; ///! low priority outgoing queue
};


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
  std::map<std::string, remote> m_remotes;

  aresolver m_aresolver;
  boost::asio::io_context& m_iocontext;
};
