#pragma once

#include "uzel/uzel.h"
#include <uzel/acculine.h>

#include <utility>
#include <boost/asio.hpp>
#include <memory>
#include <array>

class session
  : public std::enable_shared_from_this<session>
{
public:
  explicit session(boost::asio::ip::tcp::socket socket);
  ~session();

  session(const session &other) = delete;
  session(session &&other) = delete;
  session& operator=(session &&other) = delete;
  session& operator=(const session &other) = delete;

  void start();
private:
  void do_read();

  boost::asio::ip::tcp::socket m_socket;
  enum { max_length = 1024*1024 };
  std::array<char, max_length> m_data;
  uzel::MsgQueue m_inMsgQueue;
  std::string m_remoteName;
};
