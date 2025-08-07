#pragma once

#include "aresolver.h"
#include "session.h"
#include <boost/asio.hpp>
#include <map>

namespace uzel
{
  class session;

/** @brief network application base class */
class NetAppBase
{
public:
  /**
   * @brief ctor
   * @param io_context asio io context
   */
  NetAppBase(boost::asio::io_context& io_context);

  NetAppBase(const NetAppBase &) = delete;
  NetAppBase(NetAppBase &&) = delete;
  NetAppBase &operator=(const NetAppBase &) = delete;
  NetAppBase &operator=(NetAppBase &&) = delete;
  virtual ~NetAppBase() = default;

  virtual void handleServiceMsg(const Msg &msg) {}
  virtual void handleLocalMsg(const Msg &msg) {}
  virtual void handleRemoteMsg(const Msg &msg) {}
  virtual void handleLocalBroadcastMsg(const Msg &msg) {}
  virtual void handleBroadcastMsg(const Msg &msg) {}

  virtual void dispatch(Msg::shr_t msg, session::shr_t ss)
  {
    if(msg->toMe()) {
      m_dispatcher.dispatch(*msg, ss);
    }

    switch(msg->destType())
    {
      case Msg::DestType::service: {
        handleServiceMsg(*msg);
        break;
      }
      case Msg::DestType::local: {
        handleLocalMsg(*msg);
        break;
      }
      case Msg::DestType::remote: {
        handleRemoteMsg(*msg);
        break;
      }
      case Msg::DestType::localbroadcast: {
        handleLocalBroadcastMsg(*msg);
        break;
      }
      case Msg::DestType::broadcast: {
        handleBroadcastMsg(*msg);
        break;
      }
    }
  }

private:
  void serviceMsg(uzel::Msg &msg);

  aresolver m_aresolver;
  uzel::session::dispatcher_t m_dispatcher;
};

}
