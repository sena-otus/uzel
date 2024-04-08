#include "uzel.h"

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree_fwd.hpp>
#include <iostream>
#include <sstream>
#include <utility>

namespace uzel {
  Addr::Addr(std::string appname, std::string hostname)
    : m_appname(std::move(appname)), m_hostname(std::move(hostname))
  {
  }

  Msg::Msg(Msg::ptree &&header, std::string &&body)
    : m_header(header), m_body(body)
  {}

  Msg::Msg(Msg::ptree &&header, Msg::ptree &&body)
    : m_header(header), m_body(body)
  {}

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
          auto host = m_header->get<std::string>("to.h", "?");
          auto appn = m_header->get<std::string>("to.n", "?");
          std::cout << "got header of the msg for " << appn << "@"  << host << "\n";
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
    return true;
  }
}
