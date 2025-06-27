#include "inputprocessor.h"
#include "msg.h"
#include "dbg.h"

#include <boost/log/trivial.hpp>
#include <boost/property_tree/json_parser.hpp>

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
          {
            auto node = m_header->get<std::string>("from.n", "?");
            auto appn = m_header->get<std::string>("from.a", "?");
            BOOST_LOG_TRIVIAL(debug) << DBGOUTF << "got header of the msg from " << appn << "@"  << node;
          }
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
          BOOST_LOG_TRIVIAL(error) << "Closing connection because can not parse received json header " << *line;
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
      BOOST_LOG_TRIVIAL(error) << "no source appname or nodename in first message - refuse connection";
      return false;
    }
    bool isLocal = UConfigS::getUConfig().isLocalNode(node);

    if(UConfigS::getUConfig().appName() == "userver")
    {
      if(isLocal) {
        if(app == "userver") {
          BOOST_LOG_TRIVIAL(error) << "refuse loop connection userver@" << node << "<->userver@" << node;
          return false;
        }
      } else { // remote
        if(app != "userver") {
          BOOST_LOG_TRIVIAL(error) << "refuse remote connection to userver from non-userver";
          return false;
        }
      }
    } else { //  normal app
      if(!isLocal) {
        BOOST_LOG_TRIVIAL(error) << "refuse remote connecting to " << app << "@" << node;
        return false;
      }
      if(app != "userver") {
        BOOST_LOG_TRIVIAL(error) << "refuse connecting to " << app << "@" << node;
        return false;
      }
    }
    m_peer = msg.from();
    BOOST_LOG_TRIVIAL(info) << "authenticated "<< (isLocal ? "local" : "remote")
              << " connection from " << m_peer.app() << "@" << m_peer.node();
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
