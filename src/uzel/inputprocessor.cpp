#include "inputprocessor.h"
#include "msg.h"

#include <boost/property_tree/json_parser.hpp>
#include <iostream>

namespace uzel
{
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



  bool InputProcessor::auth(const uzel::Msg &msg)
  {
    auto app = msg.from().app();
    auto node = msg.from().node();
    if(app.empty() || node.empty()) {
      std::cerr << "no source appname or nodename in first message - refuse connection\n";
      return false;
    }
    bool isLocal = UConfigS::getUConfig().isLocalNode(node);

    if(UConfigS::getUConfig().appName() == "userver")
    {
      if(isLocal) {
        if(app == "userver") {
          std::cerr << "refuse loop connection userver@" << node << "<->userver@" << node<< "\n";
          return false;
        }
      } else { // remote
        if(app != "userver") {
          std::cerr << "refuse remote connection to userver from non-userver\n";
          return false;
        }
      }
    } else { //  normal app
      if(!isLocal) {
        std::cerr << "refuse remote connecting to " << app << "@" << node << "\n";
        return false;
      }
      if(app != "userver") {
        std::cerr << "refuse connecting to " << app << "@" << node << "\n";
        return false;
      }
    }
    m_peer = msg.from();
    std::cout << "authenticated "<< (isLocal ? "local" : "remote")
              << " connection from " << m_peer.app() << "@" << m_peer.node() << "\n";
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
