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
  ss->s_send_error.connect([&]() { on_session_error(ss);});
  ss->s_recv_error.connect([&]() { on_session_error(ss);});
    // who is the boss?
  if(ss->msg1().from().node() > uzel::UConfigS::getUConfig().nodeName()) {
      // he wins
      // so wait for his decision (remote message), it will tell which
      // priority or close it
    m_sessionWaitForRemote.emplace(ss);
    BOOST_LOG_TRIVIAL(debug) << "there are now " << m_sessionWaitForRemote.size()
                             << " session(s) for " << m_node << "waiting for decision";

      //    auto iprio = ss->msg1().pbody().get<int8_t>("priority");
      //    ss->setPriority(uzel::Priority::_from_integral(iprio));
  } else {
      // i win, so i decide on priorities and on fate of the session
    if(!m_sessionH)
    {
      BOOST_LOG_TRIVIAL(debug) << "got high priority connection with '" << m_node
                               << "' = " << ss;
      m_sessionH = ss;
      ss->setPriority(uzel::Priority::high);
        // it is not necessary both sides to have same priority on the channel
        // but let's do it
      uzel::Msg::ptree body{};
      body.add("priority", uzel::Priority::high);
      ss->putOutQueue(uzel::Msg(uzel::Addr(), std::move(body)));
      ss->takeOverMessages(m_outHighQueue);
    } else if(!m_sessionL) {
      BOOST_LOG_TRIVIAL(debug) << "got low priority connection with '" << m_node
                               << "' = " << ss;
      ss->setPriority(uzel::Priority::low);
      m_sessionL = ss;
      uzel::Msg::ptree body{};
      body.add("priority", uzel::Priority::low);
      ss->putOutQueue(uzel::Msg(uzel::Addr(), std::move(body)));
      ss->takeOverMessages(m_outLowQueue);
    } else {
      BOOST_LOG_TRIVIAL(debug) << "got duplicated connection with '" << m_node
                               << "' = " << ss << ", will be closed";
      ss->gracefullClose("duplicated connection");
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


void remote::on_session_error(session::shr_t ss)
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
}


void remote::send(const uzel::Msg &msg)
{
  if(!connected())
  {
      // no connection
    m_outHighQueue.emplace_back(QueuedMsg(msg));
  }
  else {
      // the very last session is the highest priority!
    m_sessionH->putOutQueue(msg);
  }
}

}
