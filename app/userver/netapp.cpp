#include "netapp.h"
#include "session.h"

using boost::asio::ip::tcp;

NetApp::NetApp(boost::asio::io_context& io_context, unsigned short port)
  : m_acceptor(io_context, tcp::endpoint(tcp::v4(), port))
{
  do_accept();
}


void NetApp::do_accept()
{
  m_acceptor.async_accept(
    [this](boost::system::error_code ec, tcp::socket socket)
      {
        if (!ec)
        {
          auto unauth = std::make_shared<session>(std::move(socket))->start();
          unauthlist.emplace_back(unauth);
        }

        do_accept();
      });
}
