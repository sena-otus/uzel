#pragma once

#include "session.h"
#include "netappcontext.h"
#include "OutgoingManager.h"

#include <boost/asio.hpp>
#include <map>

namespace uzel
{

/** incapsulates logical channel to one remote */
  class Channel
  {
  public:
    explicit Channel(NetAppContext::shr_t netctx, std::string appname);

    Channel &operator=(const Channel &) = delete;
    Channel(const Channel &other) = delete;
    Channel(Channel &&) = default;
    Channel &operator=(Channel &&) = delete;
    ~Channel() = default;

    void addSession(session::shr_t);
    void send(uzel::Msg::shr_t msg);
    [[nodiscard]] bool connected() const;
  private:
    explicit Channel(std::string &&nodeName);

    void onSessionClosed(session::shr_t ss);

    std::string m_app; ///< peer app name - known only after authentication
    std::string m_addr; ///< last known peer address
    std::unordered_set<session::shr_t> m_sessionWaitForRemote; ///!< keeps list of tcp sessions
    session::shr_t m_ss; ///< active session
    session::shr_t m_sessionL; ///< low priority session
    session::shr_t m_sessionC; ///< control session (always outgoing), if low and high are incoming
    MsgQueue m_outHighQueue; ///< high prirority outgoing message queue
    MsgQueue m_outLowQueue; ///< low priority outgoing message queue
    NetAppContext::shr_t m_netctx;
  };



  class session;

/** @brief TCP client */
  class NetLeaf
  {
  public:
      /**
       * @brief ctor
       * @param io_context asio io context
       * @param port TCP port to connect */
    NetLeaf(boost::asio::io_context& io_context, unsigned short port);

    void send(Msg::shr_t msg);
    boost::signals2::signal<void ()> s_authSuccess;

  protected:

    NetAppContextPtr netctx() { return m_netctx;}

  private:
    void start();
    void auth(session::shr_t ss);
    void serviceMsg(Msg &msg);
    void reconnectAfterDelay();
    void connectResolved( boost::system::error_code ec,  const boost::asio::ip::tcp::resolver::results_type &rezit, const std::string &hname);

    NetAppContext::shr_t m_netctx;
    Channel m_channel;
    int m_port;
    IpToSession m_ipToSession; // needed?
    OutgoingManager m_outman;
    MsgDispatcher::ScopedConnection m_priorityH;
    MsgDispatcher::ScopedConnection m_anyH;
  };

}
