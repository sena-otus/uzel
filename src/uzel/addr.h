#pragma once

#include <string>

namespace uzel
{

  class Addr
  {
  public:
    Addr() = default;
    Addr(std::string appname, std::string nodename);
      /** Same as above, but address in one string
       * @param fulladdr <appname>[@<nodename>]
       */
    Addr(const std::string &fulladdr);
    [[nodiscard]] std::string app() const {return m_appname;}
    [[nodiscard]] std::string node() const {return m_nodename;}
  private:
    Addr(const std::string &fulladdr, size_t atpos);
    std::string m_appname;
    std::string m_nodename;
  };
};
