#pragma once

#include <uzel/aresolver.h>
#include <uzel/session.h>
#include <utility>
#include <boost/asio.hpp>
#include <list>
#include <map>
#include <memory>

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
  void remoteMsg(uzel::Msg &msg);
  void broadcastMsg(uzel::Msg &msg);
  void localbroadcastMsg(uzel::Msg &msg);
  void connectResolved( boost::system::error_code ec,  boost::asio::ip::tcp::resolver::results_type rezit, const std::string &hname);
  void reconnectAfterDelay(const std::string &hname);
  void startResolving(const std::string &hname);

  boost::asio::ip::tcp::acceptor m_acceptor;
  std::map<std::string, session::shr_t> m_locals;
  std::map<std::string, session::shr_t> m_remotes;
  std::map<std::string, std::list<session::shr_t>> m_remotes2; // secondary connections

  aresolver m_aresolver;
  boost::asio::io_context& m_iocontext;
};
