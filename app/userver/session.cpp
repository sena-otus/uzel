#include "session.h"

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
  do_read();
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
  if(m_outQueue.empty()) return;
  auto self(shared_from_this());
  boost::asio::async_write(
    m_socket, boost::asio::buffer(m_outQueue.front()),
    [this, self](boost::system::error_code ec, std::size_t /*length*/)
      {
        if(ec) {
          std::cerr << "got error while sending reply: " << ec.message() << std::endl;
        } else {
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
  const bool wasEmpty = m_outQueue.empty();
  m_outQueue.emplace(msg.str());
  if(wasEmpty) {
    do_write();
  }
}

void session::putOutQueue(uzel::Msg &&msg)
{
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
