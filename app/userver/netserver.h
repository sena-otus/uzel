#pragma once

#include "remote.h"

#include <cstdint>
#include <stdexcept>
#include <unordered_map>
#include <uzel/aresolver.h>
#include <uzel/session.h>
#include <boost/asio.hpp>
#include <map>
#include <unordered_set>



namespace uzel
{
  class session;
}





BETTER_ENUM(HostStatus, int8_t, initial, resolving, resolving_error, connecting, connection_error, authenticated); //NOLINT

class RemoteHostToConnect
{
public:

  explicit RemoteHostToConnect(std::string hostname)
    : m_hostname{std::move(hostname)}, m_statusChangeTS{std::chrono::steady_clock::now()}
  {
  }

  void setStatus(HostStatus hs) {
    m_status = hs;
    m_statusChangeTS = std::chrono::steady_clock::now();
  }

  [[nodiscard]] HostStatus status() const {return m_status; }
  [[nodiscard]] std::chrono::steady_clock::time_point statusChangeTS() const {return m_statusChangeTS; }



private:
  std::string m_hostname;
  boost::asio::ip::address m_addr;
  HostStatus m_status{HostStatus::initial};
  std::chrono::steady_clock::time_point m_statusChangeTS;
};


class ConnectionManager
{
public:
  using UMap = std::unordered_map<std::string, RemoteHostToConnect>;

  template<class ...Args>
  std::pair<UMap::iterator, bool> emplace(Args&&... args)
  {
    return m_connectTo.emplace(std::forward(args)...);
  }

  void setStatus(const std::string &hname, HostStatus hs)
  {
    auto hit = m_connectTo.find(hname);
    if(hit == m_connectTo.end()) {
      throw std::runtime_error("unknown host " + hname);
    }
    hit->second.setStatus(hs);
  }

  UMap &connectMap() { return m_connectTo;}

private:
  UMap m_connectTo; ///!< map remote hostnames to be connected to their status
};


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
  void startConnecting(std::shared_ptr<boost::asio::steady_timer> timer);
    /**
     *  find channel to remote node, if it does not exist, then create a new one
     *  */
  uzel::remote &findAddRemote(const std::string &node);
  std::optional<std::string> route(const std::string &target) const;

    /** add authenticated session to remote channel */
  void addAuthSessionToRemote(const std::string &rnode, uzel::session::shr_t ss);
  void onSessionClosed(uzel::session::shr_t ss);

  const int MaxConnectionsWithAddr = 10;
  const int RefreshHostStatus_sec = 15;
  const int DelayReconnect_sec = 30;
  const unsigned ResolverThreads = 5;


  boost::asio::ip::tcp::acceptor m_acceptor;
  std::map<std::string, uzel::session::shr_t> m_locals;
  std::unordered_map<std::string, uzel::remote> m_node; ///<! map nodes to channels
  ConnectionManager m_conman;

  std::unordered_map<boost::asio::ip::address,
                     std::unordered_set<uzel::session::shr_t>,
                     AddressHash, AddressEqual> m_connectionsPerAddr;
  aresolver m_aresolver;
  boost::asio::io_context& m_iocontext;
};
