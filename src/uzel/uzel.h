#pragma once

#include <boost/asio/ip/address_v4.hpp>
#include <string>

namespace uzel
{
  class Addr
  {
  public:
    Addr(std::string appname, std::string hostname);
  private:
    std::string m_appname;
    std::string m_hostname;
  };

  class Msg
  {
  public:
    explicit Msg(Addr dest);
  private:
    Addr m_dest;
  };
};
