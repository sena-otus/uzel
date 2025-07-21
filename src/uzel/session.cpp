#include "session.h"
#include "addr.h"
#include "dbg.h"

#include <boost/asio.hpp>
#include <boost/log/trivial.hpp>


namespace uzel
{

  using boost::asio::ip::tcp;

  session::session(tcp::socket socket, Direction direction, boost::asio::ip::address ip, std::string remoteHostName)
    : m_socket(std::move(socket)),
      m_direction(direction),
      m_data{0}, m_msg1(uzel::Msg::ptree{}, ""),
      m_remoteIp(std::move(ip)),
      m_remoteHostName(std::move(remoteHostName))
  {
    m_rejected_c   = m_processor.s_rejected.connect([&](){ stop();});
    m_authorized_c = m_processor.s_auth    .connect([&](const uzel::Msg &msg){ m_msg1 = msg; s_auth(shared_from_this()); });
    m_dispatch_c   = m_processor.s_dispatch.connect([&](uzel::Msg &msg){ s_dispatch(msg);});
  }


session::~session()
{
  m_rejected_c.disconnect();
  m_authorized_c.disconnect();
  m_dispatch_c.disconnect();
  stop();
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
      self->connectHandler(ec, remote);
    }
                         );
}

void session::gracefullClose(const std::string &reason)
{
  if(m_closeFlag) {
    return;
  }
  m_reason = reason;
  m_closeFlag = true;
  if(m_outQueue.empty()) {
    sendByeAndClose();
  }
}

void session::sendByeAndClose()
{
  uzel::Msg::ptree body{};
  body.add("bye", m_reason);
  std::string &&byeStr = uzel::Msg(uzel::Addr(), std::move(body)).move_tostr();
  boost::asio::async_write(m_socket, boost::asio::buffer(byeStr),
                           [self = shared_from_this()](boost::system::error_code, std::size_t) {
                             self->stop();
                           });
}



void session::takeOverMessages(session &os)
{
  takeOverMessages(os.m_outQueue);
}


void session::takeOverMessages(MsgQueue &oq)
{
  BOOST_LOG_TRIVIAL(debug) << DBGOUT << "taking over "  << oq.size() << " messsages";
  while(!oq.empty())
  {
    if(oq.front().destType() == Msg::DestType::service) {
      continue;
    }

    m_outQueue.emplace_back(std::move(oq.front()));
    oq.pop_front();
  }
    // assume there is no write in progress when it is called... why?
    // or use if(wasempty)?
  do_write();
}



void session::connectHandler(const boost::system::error_code &ec, const tcp::resolver::results_type &remote)
{
  if(ec) {
    BOOST_LOG_TRIVIAL(error) << "connection error: " << ec.message() << " when connecting to " << remote->host_name() << "->" << remote->endpoint();
    stop();
    s_connect_error(remote->host_name());
  } else {
    start();
  }
}


void session::do_read()
{
  m_socket.async_read_some(
    boost::asio::buffer(m_data, max_length),
    [self = shared_from_this()](boost::system::error_code ec, std::size_t length)
      {
        if (!ec) {
          if(self->m_processor.processNewInput(std::string_view(self->m_data.data(), length))) {
            self->do_read();
          } else {
            BOOST_LOG_TRIVIAL(error) << " error reading from socket: " << ec.message();
            self->s_recv_error();
            self->stop();
          }
        }
      });
}

  bool session::outQueueEmpty()
  {
    auto now = std::chrono::steady_clock::now();
    int dropped {0};
    while(!m_outQueue.empty()) {
      if(now - m_outQueue.front().enqueueTime() > std::chrono::seconds(MessageTTL_sec)) {
        ++dropped;
        m_outQueue.pop_front();
      } else {
        if(dropped>0) {
          BOOST_LOG_TRIVIAL(warning) << "dropped " << dropped << " messages to '" << m_msg1.dest().node() << "' because timeout of "  << MessageTTL_sec << "sec exceeded";
        }
        return false;
      }
    }
    if(dropped>0) {
      BOOST_LOG_TRIVIAL(warning) << "dropped " << dropped << " messages to '" << m_msg1.dest().node() << "' because timeout of "  << MessageTTL_sec << "sec exceeded";
    }
    return true;
  }


//NOLINTBEGIN(misc-no-recursion)
void session::do_write()
{
  if(outQueueEmpty()) return;
  BOOST_LOG_TRIVIAL(debug) << DBGOUT << "starting async_write()...";
  boost::asio::async_write(
    m_socket, boost::asio::buffer(m_outQueue.front().msg()),
    [self = shared_from_this()](boost::system::error_code ec, std::size_t /*length*/)
      {
        if(ec) {
          BOOST_LOG_TRIVIAL(error) << "got error while writing to the socket: " << ec.message();
          self->s_send_error();
          self->stop();
       } else {
          BOOST_LOG_TRIVIAL(debug) << DBGOUT << "writing succeed " << self->m_outQueue.front();
          self->m_outQueue.pop_front();
          if(self->outQueueEmpty()) {
            if(self->m_closeFlag) {
              self->sendByeAndClose();
            }
            return;
          }
          self->do_write();
        }
      });
}
//NOLINTEND(misc-no-recursion)


void session::putOutQueue(const uzel::Msg &msg)
{
  BOOST_LOG_TRIVIAL(debug) << DBGOUT << " inserting2 message to '" << msg.dest().app() << "@" << msg.dest().node() << "' into the output queue";
  const bool wasEmpty = m_outQueue.empty();
  m_outQueue.emplace_back(msg);
  if(wasEmpty) {
    do_write();
  }
}

void session::putOutQueue(uzel::Msg &&msg)
{
  BOOST_LOG_TRIVIAL(debug) << DBGOUT << " inserting message to '" << msg.dest().app() << "@" << msg.dest().node() << "' into the output queue";
  const bool wasEmpty = m_outQueue.empty();
  {
    m_outQueue.emplace_back(msg);
    BOOST_LOG_TRIVIAL(debug) << DBGOUT << " inserted, new output queue size is: " << m_outQueue.size();
  }
  if(wasEmpty) {
    do_write();
  }
}


void session::stop()
{
  if(m_stopped) return;
  m_stopped = true;
  m_closeFlag = true;
  BOOST_LOG_TRIVIAL(debug) << DBGOUT << " session::stop()!";
  boost::system::error_code ec;

  m_socket.cancel(ec);

    // Shut down socket (this flushes buffers and signals EOF to peer)
  m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
  if (ec && ec != boost::asio::error::not_connected) {
    BOOST_LOG_TRIVIAL(error) << "Shutdown error: " << ec.message();
  }

    // Close the socket
  m_socket.close(ec);
  if (ec) {
    BOOST_LOG_TRIVIAL(error) << "Close error: " << ec.message();
  }

  s_closed(shared_from_this());
}

}
