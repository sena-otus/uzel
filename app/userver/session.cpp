#include "session.h"

#include <iostream>

using boost::asio::ip::tcp;

session::session(tcp::socket socket)
  : m_socket(std::move(socket)), m_data{0}
{
  m_processor.s_disconnect.connect(std::bind(this, disconnect));
  m_processor.s_broadcast.connect(std::bind(this, broadcastMsg));
  m_processor.s_broadcast.connect(std::bind(this, localMsg));
  m_processor.s_broadcast.connect(std::bind(this, remoteMsg));
  m_processor.s_broadcast.connect(std::bind(this, localbroadcastMsg));
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


void session::putOutQueue(const Msg &msg)
{
  bool wasEmpty = m_outQueue.empty();
  m_outQueue.emplace(msg.str());
  if(wasEmpty) {
    do_write();
  }
}


void session::disconnect()
{
  m_socket.close();
}

void session::localMsg(uzel::Msg && msg)
{
  auto lit = localConnections.find(msg.destApp());
  if(lit != localConnections.end()) {
    lit->putOutQueue(std::move(msg));
  }
}
void session::localbroadcastMsg(uzel::Msg && msg)
{
    std::for_each(localConnections.begin(), localConnections.end(),
                  [&msg](std::shared_ptr<Connection> &sc){
                    sc->putOutQueue(msg);
                  });

  }


  void session::broadcastMsg(uzel::Msg && msg)
  {
    std::for_each(remoteConnections.begin(), remoteConnections.end(),
                  [&msg](std::shared_ptr<Connection> &sc){
                    sc->putOutQueue(msg);
                    });

}
void session::remoteMsg(uzel::Msg && msg)
{
  auto rit = remoteConnections.find(msg.destHost());
  if(rit != remoteConnections.end())
  {
    rit->putOutQueue(std::move(msg));
  }
}
