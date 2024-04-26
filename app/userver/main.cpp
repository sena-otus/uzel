#include "netserver.h"

#include <iostream>
#include <stdexcept>


const int generic_errorcode = 200;


int main(int argc, char* argv[])
{
  try
  {
    boost::asio::io_context io_context;

      //NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    const NetServer server(io_context, 32300);

    io_context.run();
  }
  catch (const std::exception& ex)
  {
    std::cerr << "Exception: " << ex.what() << "\n";
    return generic_errorcode;
  }

  return 0;
}
