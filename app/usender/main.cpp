#include "uzel/addr.h"
#include "uzel/msg.h"
#include <uzel/netclient.h>

#include <iostream>
#include <utility>

/** simple app that will send a message to echo every 2 seconds */


const int generic_errorcode = 200;
namespace io = boost::asio;
const int send_s = 2;

class NetPrinter : public NetClient
{
public:
  explicit NetPrinter(boost::asio::io_context& io_context, uzel::Addr addr)
    : NetClient(io_context, 32300),  m_sendTimer(io_context, io::chrono::seconds(send_s)), m_addrto(std::move(addr))
  {
    s_authSuccess.connect([&](){
      std::cout << "auth is fired, calling sendMsg()" << std::endl;
      sendMsg();});
  }

  void dispatch(uzel::Msg &msg)
  {
    std::cout << "Got message: " << msg.str();
  }

  void sendMsg()
  {
    std::cout << "sendMsg() is called at " << __FILE_NAME__ << ": " << __LINE__ << std::endl;
    m_sendTimer.expires_after(io::chrono::seconds(send_s));
    m_sendTimer.async_wait([this](const boost::system::error_code&  /*ec*/){sendMsg();});

    uzel::Msg::ptree msgbody;
    msgbody.put("payload", "ping");
    send(uzel::Msg(m_addrto, std::move(msgbody)));
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
      std::cerr << "Usage : \n  usender <appname>[@<hostname>]" << std::endl;
      ::exit(150);
    }

      //NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    NetPrinter printerapp(io_context, uzel::Addr(argv[1]));
    io_context.run();
  }
  catch (const std::exception& ex)
  {
    std::cerr << "Exception: " << ex.what() << "\n";
    return generic_errorcode;
  }

  return 0;
}
