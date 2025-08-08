#pragma once

#include "aresolver.h"
#include "session.h"

#include <boost/asio.hpp>

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
  explicit NetAppBase(boost::asio::io_context& io_context);

  NetAppBase(const NetAppBase &) = delete;
  NetAppBase(NetAppBase &&) = delete;
  NetAppBase &operator=(const NetAppBase &) = delete;
  NetAppBase &operator=(NetAppBase &&) = delete;
  virtual ~NetAppBase() = default;

  uzel::session::dispatcher_t &dispatcher()  const;
  virtual void dispatch(Msg::shr_t msg, session::shr_t ss);

protected:
  boost::asio::io_context& iocontext() const;
  AResolver& aresolver();
private:
  virtual void handleServiceMsg(Msg::shr_t msg, session::shr_t ss) = 0;
  virtual void handleLocalMsg(Msg::shr_t msg) = 0;
  virtual void handleRemoteMsg(Msg::shr_t msg) = 0;
  virtual void handleLocalBroadcastMsg(Msg::shr_t msg) = 0;
  virtual void handleBroadcastMsg(Msg::shr_t msg) = 0;



  const unsigned ResolverThreads = 5;


  boost::asio::io_context &m_iocontext;
  AResolver m_aresolver;
  uzel::session::dispatcher_t m_dispatcher;
};

}
