#pragma once

#include "aresolver.h"
#include "session.h"
#include "netappbase.h"

#include <boost/asio.hpp>
#include <map>

namespace uzel
{
  class session;

/** @brief TCP client */
  class NetClient
  {
  public:
      /**
       * @brief ctor
       * @param io_context asio io context
       * @param port TCP port to connectect */
    NetClient(boost::asio::io_context& io_context, unsigned short port);

      // NetClient(const NetClient &) = delete;
      // NetClient(NetClient &&) = delete;
      // NetClient &operator=(const NetClient &) = delete;
      // NetClient &operator=(NetClient &&) = delete;
      // ~NetClient() override = default;

    void send(uzel::Msg::shr_t msg);
    boost::signals2::signal<void ()> s_authSuccess;

  protected:

    NetAppContextPtr netctx() { return m_netctx;}

  private:
    void start();
    void auth(uzel::session::shr_t ss);
    void do_accept();
    void serviceMsg(uzel::Msg &msg);
    void reconnectAfterDelay();
    void connectResolved( boost::system::error_code ec,  const boost::asio::ip::tcp::resolver::results_type &rezit, const std::string &hname);

    NetAppContext::shr_t m_netctx;
    std::map<std::string, uzel::session::shr_t> m_locals;
    boost::optional<boost::asio::io_context::work> m_work;
    boost::asio::steady_timer m_reconnectTimer;
    int m_port;
  };

}
