#pragma once

#include "remote.h"

#include <uzel/aresolver.h>
#include <uzel/session.h>
#include <boost/asio.hpp>
#include <map>
#include <unordered_set>



namespace uzel
{
  class session;
}


struct AddressHash {
    std::size_t operator()(const boost::asio::ip::address& addr) const {
        return std::hash<std::string>()(addr.to_string());
    }
};

struct AddressEqual {
    bool operator()(const boost::asio::ip::address& lhs, const boost::asio::ip::address& rhs) const {
        return lhs == rhs;
    }
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


  void auth(uzel::session::shr_t ss);
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
  void startConnecting(const std::string &hname);
    /**
     *  find channel to remote node, if it does not exist, then create a new one
     *  */
  uzel::remote &findAddRemote(const std::string &node);
  std::optional<std::string> route(const std::string &target) const;

    /** add authenticated session to remote channel */
  void addAuthSessionToRemote(const std::string &rnode, uzel::session::shr_t ss);
  void onSessionClosed(uzel::session::shr_t ss);

  boost::asio::ip::tcp::acceptor m_acceptor;
  std::map<std::string, uzel::session::shr_t> m_locals;
  std::map<std::string, uzel::remote> m_node; //<! map nodes to channels
  std::set<std::string> m_connecting_to;

  std::unordered_map<boost::asio::ip::address,
                     std::unordered_set<uzel::session::shr_t>,
                     AddressHash, AddressEqual> m_connectionsPerAddr;
  aresolver m_aresolver;
  boost::asio::io_context& m_iocontext;
  const int MaxConnectionsWithAddr = 10;
};
