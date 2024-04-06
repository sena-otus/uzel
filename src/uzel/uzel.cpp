#include "uzel.h"

#include <utility>

namespace uzel {
  Addr::Addr(std::string appname, std::string hostname)
    : m_appname(std::move(appname)), m_hostname(std::move(hostname))
  {
  }

  Msg::Msg(Addr dest)
    : m_dest(std::move(dest))
  {}

}
