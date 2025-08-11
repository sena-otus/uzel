#include "netappbase.h"
#include "aresolver.h"
#include "dispatcher.h"
#include "session.h"
#include "msg.h"

namespace uzel
{

  NetAppContext::NetAppContext(boost::asio::io_context& io_context)
    :  m_iocontext(io_context), m_aresolver{ResolverThreads, io_context}
  {
  }

  boost::asio::io_context& NetAppContext::iocontext() const
  {
    return m_iocontext;
  }

  AResolver& NetAppContext::aresolver()
  {
    return m_aresolver;
  }

  MsgDispatcher::shr_t NetAppContext::dispatcher() const
  {
    return m_dispatcher;
  }

}
