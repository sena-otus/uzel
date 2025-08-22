#include "NetLeaf.h"
#include "session.h"
#include "msg.h"
#include "dbg.h"

//#include "uconfig.h"

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/log/trivial.hpp>
#include <boost/ref.hpp> // NOLINT(misc-include-cleaner)
#include <boost/chrono.hpp> // NOLINT(misc-include-cleaner)
#include <boost/system/error_code.hpp>
#include <memory>
#include <string>

using boost::asio::ip::tcp;
namespace io = boost::asio;

namespace uzel
{

  const int delay_reconnect_s = 10;
  const int resolver_threads = 1;

  NetLeaf::NetLeaf(io::io_context& io_context, unsigned short port)
    : m_netctx(std::make_shared<NetAppContext>(io_context)),
      m_channel(m_netctx, "userver"),
      m_port(port),
      m_outman(m_netctx, m_ipToSession,
                [this](const std::string &)->std::optional<bool> {
                  return m_channel.connected();
               },
               port, 1)
  {
    m_outman.startConnecting("localhost");
    m_outman.start();
  }

  void NetLeaf::start()
  {
    BOOST_LOG_TRIVIAL(debug) << DBGOUT << "resolve localhost...";
    m_netctx->aresolver().async_resolve<tcp>("localhost", std::to_string(m_port),
                                             [this](const boost::system::error_code ec, const tcp::resolver::results_type resit){
                                               connectResolved(ec,resit,"localhost");
                                             });
  }

  void NetLeaf::auth(session::shr_t ss)
  {
    if(!ss->peerIsLocal()) {
      BOOST_LOG_TRIVIAL(error) << "connected to something wrong, close connnection and reconnect after delay...";
        // throw std::runtime_error("should not get remote connection here");
      ss->gracefullClose("remote connection refused");
      return;
    }

    auto appname = ss->peerApp();
    if(appname != "userver") {
      BOOST_LOG_TRIVIAL(error) << "connected to wrong app, close connnection and reconnect after delay...";
        // throw std::runtime_error("should not get remote connection here");
      ss->gracefullClose("remote connection refused");
      return;
    }

    BOOST_LOG_TRIVIAL(info) << "store local session with name " << appname;
    m_channel.addSession(ss);
  }


  void NetLeaf::send(uzel::Msg::shr_t msg)
  {
    m_channel.send(msg);
  }
}
