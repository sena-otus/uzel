#include "session.h"
#include "addr.h"
#include "dbg.h"
#include "msg.h"
#include "dump.h"
#include "netappcontext.h"

#include <boost/asio.hpp>
#include <boost/log/trivial.hpp>
#include <memory>
#include <stdexcept>
#include <string>


namespace uzel
{

  using boost::asio::ip::tcp;

  session::session(NetAppContextPtr netctx, tcp::socket socket, Direction direction, boost::asio::ip::address ip, std::string remoteHostName)
    : m_socket(std::move(socket)),
      m_direction(direction),
      m_data{0},
      m_remoteIp(std::move(ip)),
      m_remoteHostName(std::move(remoteHostName)),
      m_netctx(std::move(netctx))
  {
  }

  session::~session()
  {
      // do not call stop() from destructor, it uses shared_from_this()!
    shutdown();
  }

  bool session::peerIsLocal() const
  {
    if(!authenticated() || !m_msg1) {
      throw std::logic_error("called peerIsLocal() on non-authenticated socket");
    }
    return m_msg1->fromLocal();
  }

  const std::string& session::peerNode() const
  {
    if(!authenticated() || !m_msg1) {
      throw std::logic_error("called peerNode() on non-authenticated socket");
    }
    return m_msg1->from().node();
  }

  const std::string& session::peerApp() const
  {
    if(!authenticated() || !m_msg1) {
      throw std::logic_error("called peerApp() on non-authenticated socket");
    }
    return m_msg1->from().app();
  }

  bool session::authenticate(uzel::Msg::shr_t msg)
  {
    auto app = msg->from().app();
    auto node = msg->from().node();
    if(app.empty() || node.empty()) {
      BOOST_LOG_TRIVIAL(error) << "no source appname or nodename in first message - refuse connection";
      return false;
    }
    bool isLocal = UConfigS::getUConfig().isLocalNode(node);

    if(UConfigS::getUConfig().appName() == "userver")
    {
      if(isLocal) {
        if(app == "userver") {
          BOOST_LOG_TRIVIAL(error) << "refuse loop connection userver@" << node << "<->userver@" << node;
          return false;
        }
      } else { // remote
        if(app != "userver") {
          BOOST_LOG_TRIVIAL(error) << "refuse remote connection to userver from non-userver";
          return false;
        }
      }
    } else { //  normal app
      if(!isLocal) {
        BOOST_LOG_TRIVIAL(error) << "refuse remote connecting to " << app << "@" << node;
        return false;
      }
      if(app != "userver") {
        BOOST_LOG_TRIVIAL(error) << "refuse connecting to " << app << "@" << node;
        return false;
      }
    }
    m_msg1 = msg;
    BOOST_LOG_TRIVIAL(info) << "authenticated "<< (isLocal ? "local" : "remote")
                            << " " << m_direction._to_string()
                            << " connection from " << m_msg1->from();
    return true;
  }


  void session::dispatchMsg(Msg::shr_t msg)
  {
    if(!authenticated()) {
      if(!authenticate(msg)) {
        gracefullClose("authentication failed");
          // or stop();?
        return;
      }
      s_auth(shared_from_this());
        // no need to dispatch the first message
      return;
    }
    m_netctx->dispatcher()->dispatch(msg);
  }


  void session::start()
  {
    uzel::Msg::ptree body{};
    body.add("pid", getpid());

    putOutQueue(std::make_shared<Msg>(uzel::Addr(), "auth", std::move(body)));
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
    std::string &&byeStr = uzel::Msg(uzel::Addr(), "bye", std::move(body)).move_tostr();
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
    bool wasEmpty = outQueueEmpty();
    size_t count{0};
    while(!oq.empty())
    {
      if(oq.front().destType() == Msg::DestType::service) {
        continue;
      }

      m_outQueue.emplace_back(std::move(oq.front()));
      oq.pop_front();
      count++;
    }

    if(count > 0) {
      BOOST_LOG_TRIVIAL(debug) << DBGOUT << " queue " << &m_outQueue <<  " take over "  << count << " messsages from " << &oq;
      if(wasEmpty) {
        do_write();
      }
    }
  }



  void session::connectHandler(const boost::system::error_code &ec, const tcp::resolver::results_type &remote)
  {
    if(ec) {
      BOOST_LOG_TRIVIAL(error) << "connection error: " << ec.message() << " when connecting to " << remote->host_name() << "->" << remote->endpoint();
      s_connect_error(remote->host_name());
      stop();
    } else {
      s_connected();
      start();
    }
  }


  void session::do_read()
  {
    m_socket.async_read_some(
      boost::asio::buffer(m_data, max_length),
      [self = shared_from_this()](boost::system::error_code ec, std::size_t length)
        {
          if(ec) {
            BOOST_LOG_TRIVIAL(error) << "error reading from socket: " << ec.message();
            self->s_recv_error();
            self->stop();
            return;
          }
          if(!self->m_processor.processNewInput(std::string_view(self->m_data.data(), length), *self)) {
            BOOST_LOG_TRIVIAL(error) << "error parsing stream from remote: (implement error message)";
            self->s_recv_error();
            self->stop();
            return;
          }
          self->do_read();
        });
  }

  bool session::outQueueEmpty() const
  {
    return m_outQueue.empty();
  }

  void session::deleteOld()
  {
    auto now = std::chrono::steady_clock::now();
    int dropped {0};
    while(!m_outQueue.empty()) {
      auto etime = m_outQueue.front().enqueueTime();
      if(now - etime > std::chrono::seconds(MessageTTL_sec)) {
        ++dropped;
        m_outQueue.pop_front();
      } else {
        if(dropped>0) {
          BOOST_LOG_TRIVIAL(warning) << "dropped " << dropped << " messages to '" << m_msg1->dest().node() << "' because timeout of "  << MessageTTL_sec << "sec exceeded";
        }
        return /*false*/;
      }
    }
    if(dropped>0) {
      BOOST_LOG_TRIVIAL(warning) << "dropped " << dropped << " messages to '" << m_msg1->dest().node() << "' because timeout of "  << MessageTTL_sec << "sec exceeded";
    }
      // return true;
  }



//NOLINTBEGIN(misc-no-recursion)
  void session::do_write()
  {
    deleteOld();
    if(outQueueEmpty()) return;
    boost::asio::async_write(
      m_socket, boost::asio::buffer(m_outQueue.front().rawmsg()),
      [self = shared_from_this()](boost::system::error_code ec, std::size_t /*length*/)
        {
          if(ec) {
            BOOST_LOG_TRIVIAL(error) << "got error while writing to the socket: " << ec.message();
            self->s_send_error();
            self->stop();
          } else {
            BOOST_LOG_TRIVIAL(debug) << "session '"<< self
                                     << "': writing succeed, queue size+1: " << self->m_outQueue.size()
                                     << ", message: " << dump(self->m_outQueue.front().rawmsg());
            self->m_outQueue.pop_front();
            self->deleteOld();
              // do not wait until everything from outQueue will be sent and queue will become empty,
              // messages from queue will be taken over by the next session
              // if(self->outQueueEmpty())
            {
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


  void session::putOutQueue(uzel::Msg::shr_t msg)
  {
    const bool wasEmpty = m_outQueue.empty();
    m_outQueue.emplace_back(msg);
    BOOST_LOG_TRIVIAL(debug) << "session '"<< this << "': insert new message to " << msg->dest() << ", new output queue size is: " << m_outQueue.size();
    if(wasEmpty) {
      do_write();
    }
  }



  void session::stop()
  {
    if(m_stopped) return;

    shutdown();

      // set flags after shutdown(), so it will prevent shutdown() from
      // executing:

    m_stopped = true;
    m_closeFlag = true;

      // signal everybody interested
    s_closed(shared_from_this());
  }

  void session::shutdown()
  {
    if(m_stopped) return;
    m_stopped = true;
    m_closeFlag = true;

    boost::system::error_code ec;
    m_socket.cancel(ec);
    if (ec && ec != boost::asio::error::bad_descriptor) {
      BOOST_LOG_TRIVIAL(error) << "Cancel error: " << ec.message();
    }

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
  }


}
