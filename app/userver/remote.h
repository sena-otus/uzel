#include "uzel/netappcontext.h"
#include <uzel/session.h>

#include <string>

#pragma once

namespace uzel
{

/** incapsulates logical channel to one remote */
class remote
{
public:
  explicit remote(NetAppContext::shr_t netctx, std::string nodename);

  remote &operator=(const remote &) = delete;
  remote(const remote &other) = delete;
  remote(remote &&) = default;
  remote &operator=(remote &&) = delete;
  ~remote() = default;

  void addSession(session::shr_t);
  void send(uzel::Msg::shr_t msg);
  [[nodiscard]] bool connected() const;
  void handlePriorityMsg(const uzel::Msg &msg);
private:
  explicit remote(std::string &&nodeName);

  void onSessionClosed(session::shr_t ss);

  std::string m_node; ///< remote node name - known only after authentication
  std::string m_hname; ///< remote hostname (if known) that was used to connect to
  std::string m_addr; ///< last known remote address
  std::unordered_set<session::shr_t> m_sessionWaitForRemote; ///!< keeps list of tcp sessions
  session::shr_t m_sessionH; ///< high priority session
  session::shr_t m_sessionL; ///< low priority session
  session::shr_t m_sessionC; ///< control session (always outgoing), if low and high are incoming
  MsgQueue m_outHighQueue; ///< high prirority outgoing message queue
  MsgQueue m_outLowQueue; ///< low priority outgoing message queue
  NetAppContext::shr_t m_netctx;
};

  using NodeToSession = std::unordered_map<std::string, remote>;
}
