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
  bool wasEmpty = m_session.empty();
  m_session.push_back(ss);
  BOOST_LOG_TRIVIAL(debug) << "there are now " << m_session.size() << " session(s) for " << m_node;
  ss->s_send_error.connect([&]() { on_session_error(ss);});
  ss->s_receive_error.connect([&]() { on_session_error(ss);});
    // who is the boss?
  if(ss->msg1().from().node() > uzel::UConfigS::getUConfig().nodeName()) {
      // he wins
      // so wait for his message

      //    auto iprio = ss->msg1().pbody().get<int8_t>("priority");
      //    ss->setPriority(uzel::Priority::_from_integral(iprio));
  } else {
      // i win, so i decide on priorities and on fate of the session
    if(!m_sessionH)
    {
      ss->setPriority(uzel::Priority::high);
      m_sessionH = ss;
        // it is not necessary both sides to have same priority on the channel
        // but we can tell that other side
      uzel::Msg::ptree body{};
      body.add("priority", uzel::Priority::high);
      ss->putOutQueue(uzel::Msg(uzel::Addr(), std::move(body)));
    } else if(!m_sessionL) {
      ss->setPriority(uzel::Priority::low);
      m_sessionL = ss;
      uzel::Msg::ptree body{};
      body.add("priority", uzel::Priority::low);
      ss->putOutQueue(uzel::Msg(uzel::Addr(), std::move(body)));
    } else {
        // duplicate connection, close it
      uzel::Msg::ptree body{};
      body.add("refuse", "duplicate");
      ss->putOutQueue(uzel::Msg(uzel::Addr(), std::move(body)));
      ss->disconnect();
    }
  }


  }
  if(ss->priority() == +Priority::high) {
      // high priority
    if(m_sessionH) {
        // we have already high prio session
        // just close the new one silently
        // but send duplicate message first?
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

}
