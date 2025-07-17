#pragma once

#include "acculine.h"
#include "inputprocessor.h"
#include "msg.h"

#include <utility>
#include <boost/asio.hpp>
#include <memory>
#include <array>
#include <queue>


/**
 * class session handles one single (tcp) connection (there can be several between two remotes)
 * For incoming messages it uses uzel::Inputprocessor for initial
 * message parsing and dispatch, outgoing messages are queued in m_outQueue
 *  */
class session
  : public std::enable_shared_from_this<session>
{
public:
  using shr_t = std::shared_ptr<session>;
  using asiotcp = boost::asio::ip::tcp;

  explicit session(asiotcp::socket socket, int priority);
  ~session();

  session(const session &other) = delete;
  session(session &&other) = delete;
  session& operator=(session &&other) = delete;
  session& operator=(const session &other) = delete;

  void disconnect();
  void startConnection(const asiotcp::resolver::results_type &remote);
  void connectHandler(const boost::system::error_code &ec, const asiotcp::resolver::results_type &remote);

  boost::signals2::signal<void (session::shr_t ss)> s_auth;
  boost::signals2::signal<void (uzel::Msg &msg)> s_dispatch;
  boost::signals2::signal<void (const std::string &hostname)> s_connect_error;
  boost::signals2::signal<void ()> s_send_error;
  boost::signals2::signal<void ()> s_receive_error;


  const uzel::Msg& msg1() const {return m_msg1;}
  void start();
  void putOutQueue(const uzel::Msg& msg);
  void putOutQueue(uzel::Msg&& msg);
    // take over processing of the messages from another (old) session
  void takeOverMessages(session &os);
  void takeOverMessages(std::queue<std::string> &oq);
private:
  void do_read();
  void do_write();

  boost::asio::ip::tcp::socket m_socket;
  int m_priority{0};
  enum { max_length = 1024*1024 };
  std::array<char, max_length> m_data;
  uzel::InputProcessor m_processor;
  std::queue<std::string> m_outQueue;
  uzel::Msg m_msg1; // the very first message (used for auth)
  boost::signals2::connection m_rejected_c;
  boost::signals2::connection m_authorized_c;
  boost::signals2::connection m_dispatch_c;
};
