#include "remote.h"
#include "uzel/session.h"

#include <boost/log/trivial.hpp>

namespace uzel
{


remote::remote(std::string nodename)
  : m_node(std::move(nodename))
{
}


void remote::addSession(session::shr_t ss)
{
  ss->s_closed.connect([&](session::shr_t ss) { onSessionClosed(ss);});

  ss->s_dispatch.connect([this](uzel::Msg::shr_t msg, uzel::session::shr_t ss){
    if(msg->cname() == "priority") {
      handlePriorityMsg(msg, ss);
    }
  });

    // who is the boss?
  if(ss->msg1().from().node() > uzel::UConfigS::getUConfig().nodeName()) {
    BOOST_LOG_TRIVIAL(debug) << "he is the boss, because '"
                             << uzel::UConfigS::getUConfig().nodeName()
                             << "' < '"<< ss->msg1().from().node() << "'";
    m_sessionWaitForRemote.emplace(ss);
    BOOST_LOG_TRIVIAL(debug) << "there are now " << m_sessionWaitForRemote.size()
                             << " session(s) for " << m_node << " waiting for decision";

      //    auto iprio = ss->msg1().pbody().get<int8_t>("priority");
      //    ss->setPriority(uzel::Priority::_from_integral(iprio));
  } else {
    BOOST_LOG_TRIVIAL(debug) << "we are the boss, because '"
                             << uzel::UConfigS::getUConfig().nodeName()
                             << "' > '"<< ss->msg1().from().node() << "'";
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
    } else {
      BOOST_LOG_TRIVIAL(debug) << "got duplicated connection with '" << m_node
                               << "' = " << ss << ", will be closed";
      ss->gracefullClose("duplicated connection");
    }
  }
}

  void remote::handlePriorityMsg(Msg::shr_t msg, session::shr_t ss)
  {
    m_sessionWaitForRemote.erase(ss);
    auto prio = msg->pbody().get<Priority>("priority", Priority::undefined);
    switch(prio)
    {
      case +Priority::low:
      {
        ss->setPriority(Priority::low);
        auto sessionToClose = m_sessionL;
        m_sessionL = ss;
        if(sessionToClose) {
          sessionToClose->gracefullClose("overtaken by new connection");
        }
        break;
      }
      case +Priority::high:
      {
        ss->setPriority(Priority::high);
        auto sessionToClose = m_sessionH;
        m_sessionH = ss;
        if(sessionToClose) {
          sessionToClose->gracefullClose("overtaken by new connection");
        }
        break;
      }
      case +Priority::undefined:
      {
        ss->gracefullClose("got undefined priority from remote");
        break;
      }
    }
  }


bool remote::connected() const
{
  return m_sessionH && m_sessionL;
}

  size_t remote::sessionCount() const
  {
    return m_sessionWaitForRemote.size() + (m_sessionH ? 1 : 0) + (m_sessionL ? 1 : 0);
  }


void remote::onSessionClosed(session::shr_t ss)
{
  BOOST_LOG_TRIVIAL(debug) << "error on session " << ss << ", excluding it from list";
  for(auto ssi = m_sessionWaitForRemote.begin(); ssi != m_sessionWaitForRemote.end(); ++ssi)
  {
    if(*ssi == ss)
    {
      m_sessionWaitForRemote.erase(ssi);
      BOOST_LOG_TRIVIAL(debug) << "excluded, new list size is " << m_sessionWaitForRemote.size();
      return;
    }
  }
  if(m_sessionH == ss) m_sessionH.reset();
  if(m_sessionL == ss) m_sessionL.reset();
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
