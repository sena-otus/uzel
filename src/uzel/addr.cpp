#include "addr.h"


namespace uzel {
  Addr::Addr(std::string appname, std::string nodename)
    : m_appname(std::move(appname)), m_nodename(std::move(nodename))
  {
  }
}
