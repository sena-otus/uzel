#include "netserver.h"

#include <uzel/dbg.h>

#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/expressions.hpp>

const int generic_errorcode = 200;

void init_logging_with_flush()
{
  namespace logging = boost::log;
  namespace expr = boost::log::expressions;
  namespace attrs = boost::log::attributes;

  boost::log::add_console_log(
    std::clog,
    boost::log::keywords::auto_flush = true,
    boost::log::keywords::format = (
      expr::stream
      << "[" << expr::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y-%m-%d %H:%M:%S.%f") << "] "
      << "[" << expr::attr<attrs::current_thread_id::value_type>("ThreadID") << "] "
      << "[" << logging::trivial::severity << "] "
      << expr::smessage
                                    )
                              );

    boost::log::add_common_attributes();
}


int main(int argc, char* argv[])
{
//  init_logging_with_flush();

  try
  {
    boost::asio::io_context io_context;

      //NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    const NetServer server(io_context, 32300);

    io_context.run();
  }
  catch (const std::exception& ex)
  {
    BOOST_LOG_TRIVIAL(error) << "Exception: " << ex.what();
    return generic_errorcode;
  }

  return 0;
}
