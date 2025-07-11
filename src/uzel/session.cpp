#include "session.h"
#include "addr.h"
#include "dbg.h"

#include <boost/asio.hpp>
#include <boost/log/trivial.hpp>


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

  putOutQueue(uzel::Msg(uzel::Addr(), std::move(body)));
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


void session::takeOverMessages(session &os)
{
  takeOverMessages(os.m_outQueue);
}


void session::takeOverMessages(std::queue<std::string> &oq)
{
  BOOST_LOG_TRIVIAL(debug) << DBGOUT << "taking over "  << oq.size() << " messsages";
  while(!oq.empty())
  {
    m_outQueue.emplace(oq.front());
    oq.pop();
  }
    // assume there is no write in progress when it is called
    // or use if(wasempty)?
  do_write();
}



void session::connectHandler(const boost::system::error_code &ec, const tcp::resolver::results_type &remote)
{
  if(ec) {
    BOOST_LOG_TRIVIAL(error) << "connection error: " << ec.message() << " when connecting to " << remote->host_name() << "->" << remote->endpoint();
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
        if (!ec) {
          if(m_processor.processNewInput(std::string_view(m_data.data(), length))) {
            do_read();
          } else {
            BOOST_LOG_TRIVIAL(error) << " error reading from socket: " << ec.message();
            s_receive_error();
            disconnect();
          }
        }
      });
}

//NOLINTBEGIN(misc-no-recursion)
void session::do_write()
{
  if(m_outQueue.empty()) return;
  auto self(shared_from_this());
  BOOST_LOG_TRIVIAL(debug) << DBGOUT << "starting async_write()...";
  boost::asio::async_write(
    m_socket, boost::asio::buffer(m_outQueue.front()),
    [this, self](boost::system::error_code ec, std::size_t /*length*/)
      {
        if(ec) {
          BOOST_LOG_TRIVIAL(error) << "got error while writing to the socket: " << ec.message();
          s_send_error();
          disconnect();
       } else {
          BOOST_LOG_TRIVIAL(debug) << DBGOUT << "writing succeed " << m_outQueue.front();
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
  BOOST_LOG_TRIVIAL(debug) << DBGOUT << " inserting2 message to '" << msg.dest().app() << "@" << msg.dest().node() << "' into the output queue";
  const bool wasEmpty = m_outQueue.empty();
  m_outQueue.emplace(msg.str());
  if(wasEmpty) {
    do_write();
  }
}

void session::putOutQueue(uzel::Msg &&msg)
{
  BOOST_LOG_TRIVIAL(debug) << DBGOUT << " inserting message to '" << msg.dest().app() << "@" << msg.dest().node() << "' into the output queue";
  const bool wasEmpty = m_outQueue.empty();
  {
    auto && str = msg.move_tostr();
    BOOST_LOG_TRIVIAL(debug) << DBGOUT << " str to insert: " << str;
    m_outQueue.emplace(str);
    BOOST_LOG_TRIVIAL(debug) << DBGOUT << " inserted, new output queue size is: " << m_outQueue.size();
  }
  if(wasEmpty) {
    do_write();
  }
}


void session::disconnect()
{
  BOOST_LOG_TRIVIAL(debug) << DBGOUT << " disconnect called!";
  m_socket.close();
}
