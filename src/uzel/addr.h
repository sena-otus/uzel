#pragma once

#include <ostream>
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
    explicit Addr(const std::string &fulladdr);
    [[nodiscard]] const std::string& app() const {return m_appname;}
    [[nodiscard]] const std::string& node() const {return m_nodename;}
    friend std::ostream& operator<<(std::ostream& os, const Addr& addr);
  private:
    Addr(const std::string &fulladdr, size_t atpos);
    std::string m_appname;
    std::string m_nodename;
  };

  std::ostream &operator<<(std::ostream &os, const Addr &addr);
};
