#include "uconfig.h"

#include <boost/asio/ip/host_name.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <regex>

namespace uzel
{

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
