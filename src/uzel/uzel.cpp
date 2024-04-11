#include "uzel.h"

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree_fwd.hpp>
#include <iostream>
#include <sstream>
#include <string_view>
#include <utility>


std::string
GConfig::nodeName()
{
  return "believer";
}

GConfig& GConfig::GConfigS()
{
  static GConfig gconfig;
  return gconfig;
}



namespace uzel {
  Addr::Addr(std::string appname, std::string hostname)
    : m_appname(std::move(appname)), m_hostname(std::move(hostname))
  {
  }

  Msg::Msg(Msg::ptree &&header, std::string &&body)
    : m_header(header), m_body(body)
  {
    checkDestHost();
  }

  Msg::Msg(Msg::ptree &&header, Msg::ptree &&body)
    : m_header(header), m_body(body)
  {
    checkDestHost();
  }

  void Msg::checkDestHost()
  {
    m_destType = DestType::local;
    auto to = m_header.get_optional<std::string>("to.h");
    if(!to) return;
    if(*to == "localhost" || *to == GConfig::GConfigS().nodeName()) {
      return;
    }
    if(*to == "*") {
      m_destType = DestType::broadcast;
      return;
    }
    m_destType = DestType::remote;
    return;
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



  bool MsgQueue::processNewInput(std::string_view input)
  {
    m_acculine.addNewInput(input);
    std::optional<std::string> line;
    while((line = m_acculine.getNextCmd())) {
      if(!m_header) { // parsing msg header
        std::istringstream is(*line);
        try {
          m_header = Msg::ptree();
          boost::property_tree::read_json(is, *m_header);
          auto host = m_header->get<std::string>("from.h", "?");
          auto appn = m_header->get<std::string>("from.n", "?");
          std::cout << "got header of the msg from " << appn << "@"  << host << "\n";
            // body can be inside header for small messages
          auto bodyit = m_header->find("body");
          if(bodyit != m_header->not_found()) {
            auto body = std::move(*bodyit);
            m_mqueue.emplace(std::move(*m_header), std::move(body.second));
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
      m_mqueue.emplace(std::move(*m_header), std::move(*line));
      m_header.reset();
    }
    processQueue();
    return true;
  }

  bool MsgQueue::auth() const
  {
    return !m_remoteName.empty();
  }


  bool MsgQueue::auth(const ptree &header)
  {
    auto host = m_header->get_optional<std::string>("from.h");
    auto appn = m_header->get_optional<std::string>("from.n");
    if(!appn || !host) {
      return false;
    }
    m_remoteHost = *host;
    m_remoteName = *appn;
  }


  void MsgQueue::processQueue()
  {
    while(!m_mqueue.empty())
    {
      auto msg = m_mqueue.front();
      if(!auth()) {
        if(!auth(msg.header())) {
          disconnect();
          return;
        }
      }
      if(msg.isBroadcast())
      {
        std::for_each(remoteConnections.begin(), remoteConnections.end(),
                      [&msg](std::shared_ptr<Connection> &sc){
                        sc->putOutQueue(msg);
                      });
      } else if(msg.isRemote()) {
        auto rit = remoteConnections.find(msg.destHost());
        if(rit != remoteConnections.end())
        {
          rit->putOutQueue(std::move(msg));
        }
      }
      if(msg.isLocal() || msg.isBroadcast()) {
        if(msg.isLocalBroadcast())
        {
          std::for_each(localConnections.begin(), localConnections.end(),
                        [&msg](std::shared_ptr<Connection> &sc){
                          sc->putOutQueue(msg);
                        });
        } else {
          auto lit = localConnections.find(msg.destApp());
          if(lit != localConnections.end())
          {
            lit->putOutQueue(std::move(msg));
          }
        }
      }
      m_mqueue.pop();
    }
  }
}
