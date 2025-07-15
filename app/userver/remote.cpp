#include "remote.h"

#include <boost/log/trivial.hpp>



remote::remote(std::string nodename)
  : m_node(std::move(nodename))
{
}


void remote::addSession(session::shr_t ss)
{
  bool wasEmpty = m_session.empty();
  m_session.push_back(ss);
  BOOST_LOG_TRIVIAL(debug) << "there are now " << m_session.size() << " session(s) for " << m_node;
  ss->s_send_error.connect([&]() { on_session_error(ss);});
  ss->s_receive_error.connect([&]() { on_session_error(ss);});
  int priority = 100;
    // who is the boss?
  if(ss->msg1().from().node() > uzel::UConfigS::getUConfig().nodeName()) {
      // he is the boss
    priority = ss->msg1().priority();
  } else {
      // we decide
    priority = ss->priority();
  }

  if(priority > 0) {
      // high priority
    if(m_sessionH) {
        // we have already high prio session
        // just close the new one silently
      ss->disconnect();
    } else {
      m_sessionH = ss;
    }
  } else {
      // low priority
    if(m_sessionL) {
        // we have already low prio session
        // just close the now one silently
      ss->disconnect();
    } else {
      m_sessionL = ss;
    }
  }
  } else {
      // i am the boss
  }

    ss->takeOverMessages(m_outHighQueue);
  } else {
    // secondary connection or new connection
    // * secondary connection will have the same remote UUID
    // * new connection will have new remote UUID, that could be
    // ** remote app was restarted
    // ** remote app is a duplicate
  }
}

bool remote::connected() const
{
  return !m_session.empty();
}

void remote::on_session_error(session::shr_t ss)
{
  BOOST_LOG_TRIVIAL(debug) << "error on session " << ss << ", excluding it from list";
  for(auto ssi = m_session.begin(); ssi != m_session.end(); ++ssi)
  {
    if(*ssi == ss)
    {
      m_session.erase(ssi);
      BOOST_LOG_TRIVIAL(debug) << "excluded, new list size is " << m_session.size();
      return;
    }
  }
}


void remote::send(const uzel::Msg &msg)
{
  if(m_session.empty())
  {
      // no connection
    m_outHighQueue.emplace(msg.str());
  }
  else {
      // the very last session is the highest priority!
    m_session.back()->putOutQueue(msg);
  }
}
