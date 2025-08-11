#include <uzel/session.h>

#include <string>

#pragma once

namespace uzel
{

/** incapsulates logical channel to one remote */
class remote
{
public:
  explicit remote(std::string nodename);
//  remote(std::string node, session::shr_t ss);

  remote &operator=(const remote &) = delete;
  remote(const remote &other) = delete;
  remote(remote &&) = default;
  remote &operator=(remote &&) = delete;
  ~remote() = default;

  void addSession(session::shr_t);
  void send(uzel::Msg::shr_t msg);
  [[nodiscard]] bool connected() const;
  [[nodiscard]] size_t sessionCount() const;
  void handlePriorityMsg(const uzel::Msg &msg, uzel::session::shr_t ss);
private:
  explicit remote(std::string &&nodeName);

  void onSessionClosed(session::shr_t ss);

  std::string m_node; ///! remote node name - known only after authentication
  std::string m_hname; ///! remote hostname (if known) that was used to connect to
  std::string m_addr; ///! last known remote address
  std::unordered_set<session::shr_t> m_sessionWaitForRemote; ///! keeps list of tcp sessions
  session::shr_t m_sessionH;
  session::shr_t m_sessionL;
  MsgQueue m_outHighQueue; ///! high prirority outgoing queue
  MsgQueue m_outLowQueue; ///! low priority outgoing queue
};

  using NodeToSession = std::unordered_map<std::string, remote>;
}
