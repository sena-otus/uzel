#include "session.h"

#include <boost/asio.hpp>
#include <functional>
#include <iostream>

using boost::asio::ip::tcp;

session::session(tcp::socket socket)
  : m_socket(std::move(socket)), m_data{0}, m_msg1(uzel::Msg::ptree{}, "")
{
  m_rejected_c   = m_processor.s_rejected.connect([&](){ disconnect();});
  m_authorized_c = m_processor.s_auth    .connect([&](const uzel::Msg &msg){ m_msg1 = msg; s_auth(shared_from_this()); });
  m_dispatch_c   = m_processor.s_dispatch.connect([&](uzel::Msg &msg){ s_dispatch(msg);});
}



session::~session()
{
  m_rejected_c.disconnect();
  m_authorized_c.disconnect();
  m_dispatch_c.disconnect();
}

void session::start()
{
  uzel::Msg::ptree body{};
  body.add("auth.pid", getpid());

  putOutQueue(uzel::Msg("","", std::move(body)));
  do_read();
}

void session::startConnection(const tcp::resolver::results_type &remote)
{
  auto self(shared_from_this());
  m_socket.async_connect(
    *remote,
    [&, self, remote](const boost::system::error_code &ec) {
      this->connectHandler(ec, remote);
    }
                         );
}



void session::connectHandler(const boost::system::error_code &ec, const tcp::resolver::results_type &remote)
{
  if(ec) {
    std::cerr << "connection error: " << ec.message() << " when connecting to " << remote->host_name() << "->" << remote->endpoint()<< std::endl;
    s_connect_error(remote->host_name());
  } else {
    start();
  }
}


void session::do_read()
{
  auto self(shared_from_this());
  m_socket.async_read_some(
    boost::asio::buffer(m_data, max_length),
    [this, self](boost::system::error_code ec, std::size_t length)
      {
        if (!ec)
        {
          if(m_processor.processNewInput(std::string_view(m_data.data(), length)))
          {
            do_read();
          }
          else {
            std::cout << "close connection\n";
          }
        }
      });
}

//NOLINTBEGIN(misc-no-recursion)
void session::do_write()
{
  std::cout << "do_write is called" << std::endl;
  if(m_outQueue.empty()) return;
  auto self(shared_from_this());
  std::cout << "writing" << std::endl;
  boost::asio::async_write(
    m_socket, boost::asio::buffer(m_outQueue.front()),
    [this, self](boost::system::error_code ec, std::size_t /*length*/)
      {
        if(ec) {
          std::cerr << "got error while sending reply: " << ec.message() << std::endl;
        } else {
          std::cout << "writing succeed " << m_outQueue.front() << std::endl;
          m_outQueue.pop();
          if(m_outQueue.empty()) {
            return;
          }
          do_write();
        }
      });
}
//NOLINTEND(misc-no-recursion)


void session::putOutQueue(const uzel::Msg &msg)
{
  std::cout << "putOutQ&" << std::endl;
  const bool wasEmpty = m_outQueue.empty();
  m_outQueue.emplace(msg.str());
  if(wasEmpty) {
    do_write();
  }
}

void session::putOutQueue(uzel::Msg &&msg)
{
  std::cout << "putOutQ&&" << std::endl;
  const bool wasEmpty = m_outQueue.empty();
  m_outQueue.emplace(msg.move_tostr());
  if(wasEmpty) {
    do_write();
  }
}


void session::disconnect()
{
  m_socket.close();
}
