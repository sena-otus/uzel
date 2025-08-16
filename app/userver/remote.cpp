#include "remote.h"
#include "uzel/dbg.h"
#include "uzel/session.h"

#include <boost/log/trivial.hpp>
#include <stdexcept>

namespace uzel
{


  remote::remote(NetAppContext::shr_t netctx, std::string nodename)
    : m_netctx(std::move(netctx)), m_node(std::move(nodename))
  {
  }


  void remote::addSession(session::shr_t ss)
  {
    ss->s_closed.connect([&, ss]() { onSessionClosed(ss);});
    m_priorityH = m_netctx->dispatcher()->registerHandler("priority", [this](const Msg &msg){ handlePriorityMsg(msg); });

      // who is the boss?
    if(ss->peerNode() > uzel::UConfigS::getUConfig().nodeName()) {
      BOOST_LOG_TRIVIAL(debug) << "he is the boss, because '"
                               << uzel::UConfigS::getUConfig().nodeName()
                               << "' < '"<< ss->peerNode() << "'";
      m_sessionWaitForRemote.emplace(ss);
      BOOST_LOG_TRIVIAL(debug) << "there are now " << m_sessionWaitForRemote.size()
                               << " session(s) for " << m_node << " waiting for decision";

        //    auto iprio = ss->msg1().pbody().get<int8_t>("priority");
        //    ss->setPriority(uzel::Priority::_from_integral(iprio));
    } else {
      BOOST_LOG_TRIVIAL(debug) << "we are the boss, because '"
                               << uzel::UConfigS::getUConfig().nodeName()
                               << "' > '"<< ss->peerNode() << "'";
      if(!m_sessionH)
      {
        BOOST_LOG_TRIVIAL(debug) << "got high priority connection with '" << m_node
                                 << "' = " << ss;
        m_sessionH = ss;
        ss->setPriority(uzel::Priority::high);
          // it is not necessary both sides to have same priority on the channel
          // but let's do it
        uzel::Msg::ptree body{};
        body.add("priority", static_cast<uzel::Priority>(uzel::Priority::high));
        ss->putOutQueue(std::make_shared<uzel::Msg>(uzel::Addr(), "priority", std::move(body)));
        ss->takeOverMessages(m_outHighQueue);
      } else if(!m_sessionL) {
        BOOST_LOG_TRIVIAL(debug) << "got low priority connection with '" << m_node
                                 << "' = " << ss;
        ss->setPriority(uzel::Priority::low);
        m_sessionL = ss;
        uzel::Msg::ptree body{};
        body.add("priority", static_cast<uzel::Priority>(uzel::Priority::low));
        ss->putOutQueue(std::make_shared<uzel::Msg>(uzel::Addr(), "priority", std::move(body)));
        ss->takeOverMessages(m_outLowQueue);
      } else if(!m_sessionC &&
                m_sessionH->direction() == +Direction::incoming &&
                m_sessionL->direction() == +Direction::incoming &&
                ss->direction() == +Direction::outgoing) {
        ss->setPriority(uzel::Priority::control);
        m_sessionC = ss;
        uzel::Msg::ptree body{};
        body.add("priority", static_cast<uzel::Priority>(uzel::Priority::control));
        ss->putOutQueue(std::make_shared<uzel::Msg>(uzel::Addr(), "priority", std::move(body)));
      } else {
        BOOST_LOG_TRIVIAL(debug) << "got too many connections with '" << m_node
                                 << "' = " << ss << ", will be closed";
        ss->gracefullClose("duplicated connection");
      }
    }
  }

  void remote::handlePriorityMsg(const Msg &msg)
  {
    if (auto ss = msg.origin().lock()) {
      size_t erased = m_sessionWaitForRemote.erase(ss);
      if(ss->priority() != +Priority::undefined) {
        if(ss == m_sessionH || ss == m_sessionL) {
          BOOST_LOG_TRIVIAL(error) << m_node << ": attempt to reset session priority, ignoring...";
          return;
        }
        BOOST_LOG_TRIVIAL(error) << m_node << ": closing weird stray session " << ss;
        ss->gracefullClose("stray connection");
        return;
      }
      auto prio = msg.pbody().get<Priority>("priority", Priority::undefined);
      switch(prio)
      {
        case Priority::low:
        {
          ss->setPriority(Priority::low);
          auto sessionToClose = m_sessionL;
          m_sessionL = ss;
          BOOST_LOG_TRIVIAL(debug) << m_node << ": got low priority session";
          if(sessionToClose) {
            sessionToClose->gracefullClose("overtaken by new connection");
          }
          break;
        }
        case Priority::high:
        {
          ss->setPriority(Priority::high);
          auto sessionToClose = m_sessionH;
          m_sessionH = ss;
          BOOST_LOG_TRIVIAL(debug) << m_node << ": got high priority session";
          if(sessionToClose) {
            sessionToClose->gracefullClose("overtaken by new connection");
          }
          break;
        }
        case Priority::control:
        {
          ss->setPriority(Priority::control);
          auto sessionToClose = m_sessionC;
          m_sessionC = ss;
          BOOST_LOG_TRIVIAL(debug) << m_node << ": got control priority session";
          if(sessionToClose) {
            sessionToClose->gracefullClose("overtaken by new connection");
          }
          break;
        }
        default:
        case Priority::undefined:
        {
          BOOST_LOG_TRIVIAL(error) << m_node << ": udefined priority from remote!";
          ss->gracefullClose("got undefined priority from remote");
          break;
        }
      }
    } else {
      BOOST_LOG_TRIVIAL(warning) << m_node << ": session is gone";
    }
  }


  bool remote::connected() const
  {
    return m_sessionH && m_sessionL;
  }

  void remote::onSessionClosed(session::shr_t ss)
  {
    BOOST_LOG_TRIVIAL(debug) << "error on session " << ss << ", excluding it from waiting list";
    for(auto ssi = m_sessionWaitForRemote.begin(); ssi != m_sessionWaitForRemote.end(); ++ssi)
    {
      if(*ssi == ss)
      {
        m_sessionWaitForRemote.erase(ssi);
        BOOST_LOG_TRIVIAL(debug) << "excluded, new waiting list size is " << m_sessionWaitForRemote.size();
        return;
      }
    }
    if(m_sessionH == ss) m_sessionH.reset();
    if(m_sessionL == ss) m_sessionL.reset();
    if(m_sessionC == ss) m_sessionC.reset();
  }


  void remote::send(uzel::Msg::shr_t msg)
  {
    if(!connected())
    {
        // no connection
      m_outHighQueue.emplace_back(QueuedMsg(msg));
    } else {
      m_sessionH->putOutQueue(msg);
    }
  }

}
