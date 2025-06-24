#include "msg.h"
#include "addr.h"
#include "uconfig.h"

#include <boost/property_tree/json_parser.hpp>
#include <iostream>
#include <sstream>
#include <string_view>
#include <utility>




namespace uzel {
  Msg::Msg(Msg::ptree &&header, std::string &&body)
    : m_header(header), m_body(std::move(body)),  m_destType(DestType::local)
  {
    updateFrom();
    updateDest();
  }

  Msg::Msg(Msg::ptree &&header, Msg::ptree &&body)
    : m_header(header), m_body(body),  m_destType(DestType::local)
  {
    updateFrom();
    updateDest();
  }

  Msg::Msg(const Addr &dest, Msg::ptree && body)
    : m_destType{DestType::service}
  {
    auto localhname = uzel::UConfigS::getUConfig().nodeName();
    setFromLocal();
    auto realhname = dest.node();
    if(realhname == "localhost") {
      realhname = localhname;
    }
    if(!dest.app().empty() && !dest.node().empty())
    { // service message to peer
      m_header.add("to.n", realhname);
      m_header.add("to.a", dest.app());
    }
    m_body = Msg::ptree{};
    std::get<Msg::ptree>(m_body).swap(body);
    updateDest();
  }

  Msg::Msg(const Addr &dest, Msg &&other)
    : Msg(other)
  {
    m_dest = dest;
    setFromLocal();
  }

  void Msg::setFromLocal()
  {
    auto nodename = uzel::UConfigS::getUConfig().nodeName();
    auto appname = uzel::UConfig::appName();
    m_header.put("from.n", nodename);
    m_header.put("from.a", appname );
    m_from = Addr(appname, nodename);
  }


  std::string Msg::str() const
  {
    std::ostringstream oss;
    boost::property_tree::write_json(oss, m_header, false);
    if(m_body.index() == 0) {
      oss << std::get<std::string>(m_body) << "\n";
    } else {
      boost::property_tree::write_json(oss, std::get<ptree>(m_body), false);
    }
    return oss.str();
  }

  std::string Msg::move_tostr()
  {
    std::ostringstream oss;
    if(m_body.index() == 0) {
      boost::property_tree::write_json(oss, m_header, false);
      oss << std::get<std::string>(m_body) << "\n";
    } else {
      auto &ibody = m_header.put_child("body", ptree());
      ibody.swap(std::get<ptree>(m_body));
      boost::property_tree::write_json(oss, m_header, false);
    }
    return oss.str();
  }



  const Msg::ptree& Msg::header() const
  {
    return m_header;
  }

  Addr Msg::dest() const
  {
    return m_dest;
  }

  Addr Msg::from() const
  {
    return m_from;
  }

  void Msg::updateFrom()
  {
    m_from = Addr(m_header.get<std::string>("from.a", ""), m_header.get<std::string>("from.n", ""));
  }

  void Msg::updateDest()
  {
    m_destType = DestType::local;
    auto dest   = m_header.get<std::string>("to.n", "");
    auto appn = m_header.get<std::string>("to.a", "");
    m_dest = Addr(appn, dest);
    if(appn.empty() && dest.empty()){
      m_destType = DestType::service;
      return;
    }
    if(dest.empty()) return;
    if(dest == UConfigS::getUConfig().nodeName()) {
      if(appn == "*") {
        m_destType = DestType::localbroadcast;
      }
      return;
    }
    if(dest == "*") {
      m_destType = DestType::broadcast;
      return;
    }
    m_destType = DestType::remote;
  }

  Msg::DestType Msg::destType() const {
    return m_destType;
  }

  bool Msg::isBroadcast() const
  {
    return m_destType == DestType::broadcast;
  }

  bool Msg::isLocal() const
  {
    return m_destType == DestType::local;
  }

  bool Msg::fromLocal() const
  {
    return UConfigS::getUConfig().isLocalNode(m_from.node());
  }


  bool Msg::isRemote() const
  {
    return m_destType == DestType::remote;
  }

  bool Msg::isLocalBroadcast() const
  {
    return m_destType == DestType::localbroadcast;
  }

}
