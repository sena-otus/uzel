#include "uzel/dbg.h"
#include <boost/log/trivial.hpp>
#include <uzel/netclient.h>

#include <iostream>

/** simple echo app that will echo all messages back */


const int generic_errorcode = 200;


class NetPrinter : public NetClient
{
public:
  explicit NetPrinter(boost::asio::io_context& io_context)
    : NetClient(io_context, 32300)
  {
  }

  void dispatch(uzel::Msg &msg) override
  {
    BOOST_LOG_TRIVIAL(debug) << DBGOUTF << "Got message: " << msg.str();
    uzel::Msg responce(msg.from(), std::move(msg));
    send(std::move(responce));
  }
private:

};


int main(int argc, char* argv[])
{
  try
  {
    boost::asio::io_context io_context;

      //NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    NetPrinter printerapp(io_context);
    io_context.run();
  }
  catch (const std::exception& ex)
  {
    BOOST_LOG_TRIVIAL(error) << "Exception: " << ex.what();
    return generic_errorcode;
  }

  return 0;
}
