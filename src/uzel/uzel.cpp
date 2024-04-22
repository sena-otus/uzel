#include "uzel.h"
#include "uconfig.h"


#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree_fwd.hpp>
#include <iostream>
#include <sstream>
#include <string_view>
#include <utility>




namespace uzel {
  Addr::Addr(std::string appname, std::string nodename)
    : m_appname(std::move(appname)), m_nodename(std::move(nodename))
  {
  }


  Msg::Msg(Msg::ptree &&header, std::string &&body)
    : m_header(header), m_body(body),  m_destType(DestType::local)
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

  Msg::Msg(const std::string &appname, const std::string &hname, Msg::ptree && body)
    : m_destType{DestType::service}
  {
    auto localhname = uzel::UConfigS::getUConfig().nodeName();
    m_header.add("from.n", localhname);
    m_header.add("from.a", uzel::UConfig::appName());
    auto realhname = hname;
    if(realhname == "localhost") {
      realhname = localhname;
    }
    if(!appname.empty() && !hname.empty())
    { // service message to peer
      m_header.add("to.n", realhname);
      m_header.add("to.a", appname);
    }
    m_body = Msg::ptree{};
    std::get<Msg::ptree>(m_body).swap(body);
    updateDest();
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

  const Addr& Msg::dest() const
  {
    return m_dest;
  }

  const Addr& Msg::from() const
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

  bool Msg::isRemote() const
  {
    return m_destType == DestType::remote;
  }

  bool Msg::isLocalBroadcast() const
  {
    return m_destType == DestType::localbroadcast;
  }

  bool InputProcessor::processNewInput(std::string_view input)
  {
    m_acculine.addNewInput(input);
    std::optional<std::string> line;
    while((line = m_acculine.getNextCmd())) {
      if(!m_header) { // parsing msg header
        std::istringstream is(*line);
        try {
          m_header = Msg::ptree();
          boost::property_tree::read_json(is, *m_header);
          // auto host = m_header->get<std::string>("from.h", "?");
          // auto appn = m_header->get<std::string>("from.n", "?");
          // std::cout << "got header of the msg from " << appn << "@"  << host << "\n";
            // body can be inside header for small messages
          auto bodyit = m_header->find("body");
          if(bodyit != m_header->not_found()) {
            Msg::ptree body;
            body.swap(bodyit->second);
            m_header->erase("body");
            processMsg(Msg(std::move(*m_header), std::move(body)));
          }
          m_header.reset();
        }
        catch (...) {
          std::cerr << "Closing connection because can not parse received json header " << *line << "\n";
          return false;
        }
        continue;
      }
        // create msg
      processMsg(Msg(std::move(*m_header), std::move(*line)));
      m_header.reset();
    }
    return true;
  }

  bool InputProcessor::auth() const
  {
    return !m_peer.node().empty();
  }


  bool InputProcessor::isLocal() const
  {
    return m_isLocal;
  }


  bool InputProcessor::auth(const uzel::Msg &msg)
  {
    auto app = msg.from().app();
    auto node = msg.from().node();
    if(app.empty() || node.empty()) {
      std::cerr << "no source appname or nodename in first message - refuse connection\n";
      return false;
    }
    m_isLocal = UConfigS::getUConfig().isLocalNode(node);

    if(m_isLocal) {
      if(app == "userver") {
        std::cerr << "refuse loop connection\n";
        return false;
      }
    } else {
      if(app != "userver") {
        std::cerr << "refuse remote connection from non-userver\n";
        return false;
      }
    }
    m_peer = msg.from();
    return true;
  }


  void InputProcessor::processMsg(Msg && msg)
  {
    if(!auth()) {
      if(!auth(msg)) {
        s_rejected();
        return;
      }
      s_auth(msg);
    }
    s_dispatch(msg);
  }
}
