#pragma once

#include "aresolver.h"
#include "dispatcher.h"

#include <boost/asio.hpp>
#include <memory>

namespace uzel
{
  class session;
  using SessionPtr = std::shared_ptr<session>;
  class Msg;
  using MsgPtr = std::shared_ptr<Msg>;

/** @brief network application context class */
class NetAppContext final
{
public:
  using shr_t = std::shared_ptr<NetAppContext>;
  /**
   * @brief ctor
   * @param io_context asio io context
   */
  explicit NetAppContext(boost::asio::io_context& io_context);

  NetAppContext(const NetAppContext &) = delete;
  NetAppContext(NetAppContext &&) = delete;
  NetAppContext &operator=(const NetAppContext &) = delete;
  NetAppContext &operator=(NetAppContext &&) = delete;
  virtual ~NetAppContext() = default;

  MsgDispatcher::shr_t dispatcher()  const;
  void dispatch(MsgPtr msg, SessionPtr ss);

  boost::asio::io_context& iocontext() const;
  AResolver& aresolver();
private:
  const unsigned ResolverThreads = 5;


  boost::asio::io_context &m_iocontext;
  AResolver m_aresolver;
  MsgDispatcher::shr_t m_dispatcher;
};

}
