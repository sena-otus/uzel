#include <uzel/session.h>

#include <string>

#pragma once

/** incapsulates logical channel to one remote */
class remote
{
public:
  explicit remote(std::string name);

  remote &operator=(const remote &) = delete;
  remote(const remote &other) = delete;
  remote(remote &&) = default;
  remote &operator=(remote &&) = default;

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
