#pragma once

#include "remote.h"
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
  [[nodiscard]] std::string hostname() const {return m_hostname;}

  void setAddr(const boost::asio::ip::address &addr) {
    m_addr=addr;
  }


private:
  std::string m_hostname;
  boost::asio::ip::address m_addr;
  HostStatus m_status{HostStatus::initial};
  std::chrono::steady_clock::time_point m_statusChangeTS;
  size_t m_sessions{0};
};


/** Maintain outgoing connections to remote hosts (no listen/accept here) */
class OutgoingManager final
{
public:
  explicit OutgoingManager(
    boost::asio::io_context& iocontext,
    AResolver &resolver,
    const uzel::IpToSession &ipToSession,
    const uzel::NodeToSession &nodeToSession);

  OutgoingManager(OutgoingManager &&) = delete;
  OutgoingManager &operator=(const OutgoingManager &) = delete;
  OutgoingManager &operator=(OutgoingManager &&) = delete;
  OutgoingManager(const OutgoingManager &other) = delete;
  ~OutgoingManager() = default;

  using UMap = std::unordered_map<std::string, RemoteHostToConnect>;

  void startConnecting(const std::string &hname);
  void startConnecting();

    // NOLINTBEGIN(misc-non-private-member-variables-in-classes,cppcoreguidelines-non-private-member-variables-in-classes)
  boost::signals2::signal<void (uzel::session::shr_t unauth)> s_sessionCreated;
    // NOLINTEND(misc-non-private-member-variables-in-classes,cppcoreguidelines-non-private-member-variables-in-classes)


private:
  void startConnecting(std::shared_ptr<boost::asio::steady_timer> timer);
  void startResolving(RemoteHostToConnect &rh);
  void connectResolved(const boost::system::error_code ec, const boost::asio::ip::tcp::resolver::results_type rezit, RemoteHostToConnect &rh);


// continue private section: variable members
  boost::asio::io_context& m_iocontext;
  AResolver &m_aresolver;
  const uzel::IpToSession &m_ipToSession;
  const uzel::NodeToSession &m_nodeToSession;
  UMap m_connectTo; ///!< map remote hostnames to be connected to their status
  const int RefreshHostStatus_sec{15};
  const int DelayReconnect_sec{30};
};


}
