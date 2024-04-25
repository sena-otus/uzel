#pragma once

#include "uzel.h"
#include "acculine.h"

#include <utility>
#include <boost/asio.hpp>
#include <memory>
#include <array>



class session
  : public std::enable_shared_from_this<session>
{
public:
  using shr_t = std::shared_ptr<session>;

  explicit session(boost::asio::ip::tcp::socket socket);
  ~session();

  session(const session &other) = delete;
  session(session &&other) = delete;
  session& operator=(session &&other) = delete;
  session& operator=(const session &other) = delete;

  void disconnect();
  void startConnection(const boost::asio::ip::tcp::resolver::results_type &remote);
  void connectHandler(const boost::system::error_code &ec);

  boost::signals2::signal<void (session::shr_t ss)> s_auth;
  boost::signals2::signal<void (uzel::Msg &msg)> s_dispatch;


  const uzel::Msg& msg1() const {return m_msg1;}
  void start();
  void putOutQueue(const uzel::Msg& msg);
  void putOutQueue(uzel::Msg&& msg);
private:
  void do_read();
  void do_write();

  boost::asio::ip::tcp::socket m_socket;
  enum { max_length = 1024*1024 };
  std::array<char, max_length> m_data;
  uzel::InputProcessor m_processor;
  std::queue<std::string> m_outQueue;
  uzel::Msg m_msg1; // the very first message (used for auth)
  boost::signals2::connection m_rejected_c;
  boost::signals2::connection m_authorized_c;
  boost::signals2::connection m_dispatch_c;
};
