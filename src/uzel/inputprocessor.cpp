#include "inputprocessor.h"
#include "msg.h"
#include "dbg.h"
#include "session.h"

#include <boost/log/trivial.hpp>
#include <boost/property_tree/json_parser.hpp>

namespace uzel
{
  bool InputProcessor::processNewInput(std::string_view input, session &ss)
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
            BOOST_LOG_TRIVIAL(debug) << "got header of the msg from <" << appn << "@"  << node << ">: '" << *line << "'";
          }
            // body can be inside header for small messages
          auto bodyit = m_header->find("body");
          if(bodyit != m_header->not_found()) {
            Msg::ptree body;
            body.swap(bodyit->second);
            m_header->erase("body");
            ss.dispatchMsg(std::make_shared<Msg>(std::move(*m_header), std::move(body), ss.weak_from_this()));
            m_header.reset();
          }
        }
        catch (...) {
          BOOST_LOG_TRIVIAL(error) << "Closing connection because can not parse received json header " << *line;
          return false;
        }
        continue;
      }
        // create msg
      ss.dispatchMsg(std::make_shared<Msg>(std::move(*m_header), std::move(*line), ss.weak_from_this()));
      m_header.reset();
    }
    return true;
  }
}
