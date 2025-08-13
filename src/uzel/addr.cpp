#include "addr.h"

#include <string>
#include <utility>

namespace uzel {
  Addr::Addr(std::string appname, std::string nodename)
    : m_appname(std::move(appname)), m_nodename(std::move(nodename))
  {
  }

  Addr::Addr(const std::string &fulladdr, const size_t atpos)
    : Addr(fulladdr.substr(0, atpos), (atpos == std::string::npos) ? "" : fulladdr.substr(atpos+1))
  {
  }


  Addr::Addr(const std::string &fulladdr)
    : Addr(fulladdr, fulladdr.find('@'))
  {
  }

  std::ostream &operator<<(std::ostream &os, const Addr &addr) {
    if (addr.m_appname.empty() && addr.m_nodename.empty()) {
      os << "<>";
      return os;
    }
    os << '<' << addr.m_appname << '@' << addr.m_nodename << '>';
    return os;
  }

} // namespace uzel
