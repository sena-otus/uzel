#pragma once

#include <string>

namespace uzel
{

  class Addr
  {
  public:
    Addr() = default;
    Addr(std::string appname, std::string nodename);
    [[nodiscard]] std::string app() const {return m_appname;}
    [[nodiscard]] std::string node() const {return m_nodename;}
  private:
    std::string m_appname;
    std::string m_nodename;
  };
};
