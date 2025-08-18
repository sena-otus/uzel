#pragma once

#include "remote.h"
#include "uzel/netappcontext.h"
#include "uzel/session.h"
#include <uzel/enum.h>
#include <uzel/aresolver.h>
#include <boost/asio.hpp>
#include <chrono>


BETTER_ENUM(HostStatus, int8_t, initial, resolving, resolving_error, connecting, connection_error, connected, closed); //NOLINT

namespace uzel {

    /** Status of the remote host to connect to */
  class RemoteHostToConnect
  {
  public:
    using shr_t = std::shared_ptr<RemoteHostToConnect>;
    explicit RemoteHostToConnect(std::string hostname, std::string service)
      : m_hostname{std::move(hostname)}, m_service(std::move(service)),
        m_statusChangeTS{std::chrono::steady_clock::now()}
      {
      }

    void setStatus(HostStatus hs) {
      m_status = hs;
      m_statusChangeTS = std::chrono::steady_clock::now();
    }

    [[nodiscard]] HostStatus status() const {return m_status; }
    [[nodiscard]] std::chrono::steady_clock::time_point statusChangeTS() const {return m_statusChangeTS; }
    [[nodiscard]] const std::string& hostname() const {return m_hostname;}
    [[nodiscard]] const std::string& service() const {return m_service;}

    void setAddr(const boost::asio::ip::address &addr) {
      m_addr=addr;
    }


  private:
    std::string m_hostname;
    std::string m_service;
    boost::asio::ip::address m_addr;
    HostStatus m_status{HostStatus::initial};
    std::chrono::steady_clock::time_point m_statusChangeTS;
    size_t m_sessions{0};
  };


    /** Maintain outgoing connections to remote hosts (no listen/accept here) */
  class OutgoingManager final
  {
  public:
    using UMap = std::unordered_map<std::string, RemoteHostToConnect::shr_t>;

    explicit OutgoingManager(
      NetAppContext::shr_t,
      const uzel::IpToSession &ipToSession,
      const uzel::NodeToSession &nodeToSession,
      unsigned port);

    OutgoingManager(OutgoingManager &&) = delete;
    OutgoingManager &operator=(const OutgoingManager &) = delete;
    OutgoingManager &operator=(OutgoingManager &&) = delete;
    OutgoingManager(const OutgoingManager &other) = delete;
    ~OutgoingManager() = default;


    void startConnecting(const std::string &hname);

      /** call to start outgoing connections */
    void start();

      /**
       * Pull the next tick forward to "now".
       * Call this from anywhere (any thread) when a connection just closed
       **/
    void poke();

      // NOLINTBEGIN(misc-non-private-member-variables-in-classes,cppcoreguidelines-non-private-member-variables-in-classes)
    boost::signals2::signal<void (uzel::session::shr_t unauth)> s_sessionCreated;
      // NOLINTEND(misc-non-private-member-variables-in-classes,cppcoreguidelines-non-private-member-variables-in-classes)


  private:
    void scheduleNext(int seconds);
    void startConnecting();
    void startResolving(RemoteHostToConnect::shr_t rh);
    void connectResolved(const boost::system::error_code ec, const boost::asio::ip::tcp::resolver::results_type rezit, RemoteHostToConnect::shr_t rh);


// continue private section: variable members
    NetAppContext::shr_t m_netctx;
    boost::asio::strand<boost::asio::any_io_executor> m_strand;
    boost::asio::steady_timer m_timer;
    const uzel::IpToSession &m_ipToSession;
    const uzel::NodeToSession &m_nodeToSession;
    UMap m_connectTo; ///!< map remote hostnames to be connected to their status
    unsigned m_port;
    const int RefreshHostStatus_sec{15};
    const int DelayReconnect_sec{30};
    const int ActionTimeout_sec{15};
  };


}
