#pragma once

#include "netappcontext.h"
#include "session.h"
#include "enum.h"

#include <boost/log/trivial.hpp>
#include <boost/asio.hpp>
#include <chrono>
#include <utility>


BETTER_ENUM(HostStatus, int8_t, initial, resolving, resolving_error, connecting, connection_error, connected, closed); //NOLINT

namespace uzel {

    /** Status of the remote host to connect to */
  class RemoteHostToConnect
  {
  public:
    using shr_t = std::shared_ptr<RemoteHostToConnect>;
    explicit RemoteHostToConnect(std::string hostname, std::string service, unsigned wantedConnections)
      : m_hostname{std::move(hostname)}, m_service(std::move(service)),
        m_statusChangeTS{std::chrono::steady_clock::now()},
        m_wantedConnections(wantedConnections)
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

    [[nodiscard]] unsigned wantedConnections() const {return m_wantedConnections;}

  private:
    std::string m_hostname;
    std::string m_service;
    boost::asio::ip::address m_addr;
    HostStatus m_status{HostStatus::initial};
    std::chrono::steady_clock::time_point m_statusChangeTS;
    unsigned m_wantedConnections{2};
  };


    /** Maintain outgoing connections to remote hosts (no listen/accept here) */
  class OutgoingManager final
  {
  public:
    using UMap = std::unordered_map<std::string, RemoteHostToConnect::shr_t>;

      /**
       * Callback function that is passed to the constructor, it
       * answers the question, if given node name is completely
       * connected and authenticated.
       *
       *  - If remote node with the given is not known, then return
       *  value is not set.
       *  - If remote node is known, but is not completely connected,
       *  for example not all necessary connections are authenticated or
       *  established, then returned value is set to false.
       *  - If remote node is completely connected, e.g. all necessary
       *  connections are established, authenticated and initialized,
       *  then returned value is set to true.
       *  */
    using ConnectedFn = std::function<std::optional<bool>(const std::string&)>;

      /**
       * Constructs OutgoingManager.
       * @param netctx networking context
       * @param ipTosession reference to IpToSession map
       * @param connectedFn callback to check if remote node is already
       * connected, see also ConnectedFn
       * @param port remote port to connect
       * @param wantedConnections how many connections should be
       * established with remote host by default (may vary from host to
       * host), usually 1 or 2
       * */
    explicit OutgoingManager(
      NetAppContext::shr_t netctx,
      const IpToSession &ipToSession,
      ConnectedFn connectedFn,
      unsigned port, unsigned wantedConnections);

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
    ConnectedFn m_connectedFn;
    UMap m_connectTo; ///!< map remote hostnames to be connected to their status
    unsigned m_port;
    unsigned m_wantedConnections;
    const int RefreshHostStatus_sec{15};
    const int DelayReconnect_sec{30};
    const int ActionTimeout_sec{15};
  };


}
