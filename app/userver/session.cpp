#include "session.h"

using boost::asio::ip::tcp;

session::session(tcp::socket socket, unsigned N)
  : m_socket(std::move(socket)), m_data{0}
{
}

session::~session()
{
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
            //         async::receive(m_handle, m_data.data(), length);
          do_read();
        }
      });
}
