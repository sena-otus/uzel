#include "msg.h"
#include "addr.h"
#include "uconfig.h"

#include <boost/property_tree/json_parser.hpp>
#include <ios>
#include <iostream>
#include <sstream>
#include <string_view>
#include <utility>
#include <variant>




namespace uzel {
  Msg::Msg(Msg::ptree &&header, std::string &&body)
    : m_body{std::vector<char>{body.begin(), body.end()}},  m_destType(DestType::local)
  {
    m_header.swap(header);
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
    setFromLocal();
    m_body = Msg::ptree{};
    std::get<Msg::ptree>(m_body).swap(body);
    setDest(dest);
  }

  Msg::Msg(const Addr &dest, Msg &&other)
    : Msg(other)
  {
    setFromLocal();
    setDest(dest);
  }

  void Msg::setDest(const Addr &dest) {
    auto realhname = dest.node();
    if(realhname == "localhost") {
      realhname = uzel::UConfigS::getUConfig().nodeName();
    }
    if(!dest.app().empty() && !dest.node().empty())
    { // service message to peer
      m_header.put("to.n", realhname);
      m_header.put("to.a", dest.app());
    }
    updateDest();
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
    if(m_body.index() == 0) {
      boost::property_tree::write_json(oss, m_header, false);
      const auto & vbody = std::get<std::vector<char>>(m_body);
        // TODO: use memcpy
      oss.write(vbody.data(), static_cast<std::streamsize>(vbody.size()));
      oss << "\n";
    } else {
      boost::property_tree::write_json(oss, m_header, false);
      boost::property_tree::write_json(oss, std::get<ptree>(m_body), false);
        // or embed the body into the header:
        // auto fullmsg = m_header;
        // fullmsg.put_child("body", std::get<ptree>(m_body));
        // boost::property_tree::write_json(oss, fullmsg, false);
    }
    return oss.str();
  }

  std::string Msg::move_tostr()
  {
    std::ostringstream oss;
    if(m_body.index() == 0) {
      boost::property_tree::write_json(oss, m_header, false);
      const auto & vbody = std::get<std::vector<char>>(m_body);
        // TODO: use memcpy
      oss.write(vbody.data(), static_cast<std::streamsize>(vbody.size()));
      oss << "\n";
    } else {
      auto &ibody = m_header.put_child("body", ptree());
      ibody.swap(std::get<ptree>(m_body));
      boost::property_tree::write_json(oss, m_header, false);
    }
    return oss.str();
  }

  std::vector<char> Msg::charvec() const
  {
      // TODO: FIXME: that is inefficient, store directly to std::vector!
    std::string tmpstr = str();
    return {tmpstr.begin(), tmpstr.end()};
  }

  std::vector<char> Msg::moveToCharvec()
  {
      // TODO: FIXME: that is inefficient, store directly to std::vector!
    std::string str = move_tostr();
    return {str.begin(), str.end()};
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



  class vector_inputbuf : public std::streambuf {
  public:
    explicit vector_inputbuf(const std::vector<char>& data) {
        // const_cast is necessary to workaround setg() flaw
      char* begin = const_cast<char*>(data.data()); // NOLINT{cppcoreguidelines-pro-type-const-cast}
      setg(begin, begin, begin + data.size()); // NOLINT{cppcoreguidelines-pro-bounds-pointer-arithmetic}
    }
  };

  const Msg::ptree& Msg::pbody() const
  {
    if(std::holds_alternative<Msg::ptree>(m_body))
    {
      return std::get<Msg::ptree>(m_body);
    }
    if(std::holds_alternative<std::vector<char>>(m_body)) {
      auto bodyvec(std::move(std::get<std::vector<char>>(m_body)));
      m_body = Msg::ptree();
      vector_inputbuf vibuf(bodyvec);
      std::istream is(&vibuf);
      auto& pbody = std::get<Msg::ptree>(m_body);
      boost::property_tree::read_json(is, pbody);
      return pbody;
    }
    throw std::runtime_error("unsupported message body format");
  }
}
