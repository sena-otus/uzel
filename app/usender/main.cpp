#include "uzel/addr.h"
#include "uzel/msg.h"
#include "uzel/dbg.h"
#include "uzel/session.h"
#include <boost/log/trivial.hpp>
#include <memory>
#include <uzel/netclient.h>

#include <utility>

/** simple app that will send a message to echo every 2 seconds */


const int generic_errorcode = 200;
namespace io = boost::asio;
const int send_s = 2;

class NetPrinter : public uzel::NetClient
{
public:
  explicit NetPrinter(boost::asio::io_context& io_context, uzel::Addr addr)
    : NetClient(io_context, 32300),  m_sendTimer(io_context, io::chrono::seconds(send_s)), m_addrto(std::move(addr))
    {
      s_authSuccess.connect([&](){
        BOOST_LOG_TRIVIAL(debug) << DBGOUT << "auth is fired, calling sendMsg()";
      sendMsg();});
    }

  void dispatch(uzel::Msg::shr_t msg, uzel::session::shr_t)
  {
    BOOST_LOG_TRIVIAL(debug) << "Got message: " << msg->str();
  }

  void sendMsg()
  {
    BOOST_LOG_TRIVIAL(debug) << DBGOUTF;
    m_sendTimer.expires_after(io::chrono::seconds(send_s));
    m_sendTimer.async_wait([this](const boost::system::error_code&  /*ec*/){sendMsg();});

    uzel::Msg::ptree msgbody;
    msgbody.put("payload", "ping");
    send(std::make_shared<uzel::Msg>(m_addrto, "ping", std::move(msgbody)));
  }
private:
  boost::asio::steady_timer m_sendTimer;
  uzel::Addr m_addrto;
};


int main(int argc, char* argv[])
{
  try
  {
    boost::asio::io_context io_context;
    if(argc < 2)
    {
      BOOST_LOG_TRIVIAL(error) << "Usage : \n  usender <appname>[@<hostname>]";
      ::exit(150);
    }

      //NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    NetPrinter printerapp(io_context, uzel::Addr(argv[1]));
    io_context.run();
  }
  catch (const std::exception& ex)
  {
    BOOST_LOG_TRIVIAL(error) << "Exception: " << ex.what();
    return generic_errorcode;
  }

  return 0;
}
