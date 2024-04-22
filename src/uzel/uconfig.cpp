#include "uconfig.h"

#include <boost/dll/runtime_symbol_info.hpp>
#include <boost/asio/ip/host_name.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/system/error_code.hpp>
#include <boost/filesystem.hpp>
#include <regex>

namespace uzel
{

  std::string executable_name()
  {
#if defined(PLATFORM_POSIX) || defined(__linux__) //check defines for your setup

    std::string sp;
    std::ifstream("/proc/self/comm") >> sp;
    return sp;

#elif defined(_WIN32)

    char buf[MAX_PATH];
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    return buf;

#else

    static_assert(false, "unrecognized platform");

#endif
}

  UConfig::UConfig()
  {
    boost::property_tree::ini_parser::read_ini("uzel.ini", m_pt);
  }

  std::string
  UConfig::nodeName() const
  {
      // get hostname
    return boost::asio::ip::host_name();
  }

  std::string
  UConfig::appName()
  {
      //   boost::system::error_code ec;
      //const boost::dll::fs::path fp = boost::dll::program_location(ec);
      //return fp.filename().string();
    return executable_name();
  }


  UConfig& UConfigS::getUConfig()
  {
    static UConfig uconfig;
    return uconfig;
  }

  bool UConfig::isLocalNode(const std::string &nname) const
  {
    return (nname == nodeName());
  }

  std::list<std::string> UConfig::remotes() const
  {
    auto rs = m_pt.get<std::string>("remotes.name");
    static const std::regex reg("\\s*([,])\\s*");
    const std::sregex_token_iterator iter(rs.begin(), rs.end(), reg, -1);
    const std::sregex_token_iterator end;
    std::list<std::string> lst(iter, end);
    return lst;
  }
};
