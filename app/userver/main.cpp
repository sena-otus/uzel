#include "netapp.h"

#include <iostream>
#include <stdexcept>


const int generic_errorcode = 200;


int main(int argc, char* argv[])
{
  try
  {
    if (argc != 3)
    {
      std::cout << "Usage\n"
                <<"  bulk_server <tcp port> <block size>\n"
                << "\n"
                << "Where"
                << "  <tcp port>   is tcp port to listen for incoming connections\n"
                << "  <block size> is command block size\n"
                << "\n"
                << "Example"
                << "  bulk_server 9300 3\n";
      return 0;
    }

    boost::asio::io_context io_context;

      //NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    const NetApp server(io_context, std::stoul(argv[1]), std::stoul(argv[2]));

    io_context.run();
  }
  catch (const std::exception& ex)
  {
    std::cerr << "Exception: " << ex.what() << "\n";
    return generic_errorcode;
  }

  return 0;
}
