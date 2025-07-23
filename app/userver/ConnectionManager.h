#pragma once

#include <uzel/enum.h>
#include <uzel/aresolver.h>
#include <boost/asio.hpp>
#include <chrono>


BETTER_ENUM(HostStatus, int8_t, initial, resolving, resolving_error, connecting, connection_error, authenticated, closed); //NOLINT

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
};


/** Maintain outgoing connections to remote hosts (no listen/accept here) */
class ConnectionManager final
{
public:
  explicit ConnectionManager(boost::asio::io_context& iocontextw, aresolver &resolver);

  ConnectionManager(ConnectionManager &&) = delete;
  ConnectionManager &operator=(const ConnectionManager &) = delete;
  ConnectionManager &operator=(ConnectionManager &&) = delete;
  ConnectionManager(const ConnectionManager &other) = delete;
  ~ConnectionManager() = default;

  using UMap = std::unordered_map<std::string, RemoteHostToConnect>;

  void startConnecting(const std::string &hname);
  void startConnecting();

private:
  void startConnecting(std::shared_ptr<boost::asio::steady_timer> timer);
  void startResolving(RemoteHostToConnect &rh);
  void connectResolved(const boost::system::error_code ec, const boost::asio::ip::tcp::resolver::results_type rezit, RemoteHostToConnect &rh);


private:
  boost::asio::io_context& m_iocontext;
  aresolver &m_aresolver;
  UMap m_connectTo; ///!< map remote hostnames to be connected to their status
  const int RefreshHostStatus_sec{15};
  const int DelayReconnect_sec{30};
};
