#include "netappbase.h"
#include "aresolver.h"
#include "session.h"
#include "msg.h"

namespace uzel
{

  NetAppBase::NetAppBase(boost::asio::io_context& io_context)
    :  m_iocontext(io_context), m_aresolver{ResolverThreads, io_context}
  {
  }


  boost::asio::io_context& NetAppBase::iocontext() const
  {
    return m_iocontext;
  }

  AResolver& NetAppBase::aresolver()
  {
    return m_aresolver;
  }


  void NetAppBase::dispatch(Msg::shr_t msg, session::shr_t ss)
  {
    if(msg->toMe()) {
      m_dispatcher.dispatch(*msg, ss);
    }

    switch(msg->destType())
    {
      case Msg::DestType::service: {
        handleServiceMsg(msg, ss);
        break;
      }
      case Msg::DestType::local: {
        handleLocalMsg(msg);
        break;
      }
      case Msg::DestType::remote: {
        handleRemoteMsg(msg);
        break;
      }
      case Msg::DestType::localbroadcast: {
        handleLocalBroadcastMsg(msg);
        break;
      }
      case Msg::DestType::broadcast: {
        handleBroadcastMsg(msg);
        break;
      }
    }
  }

}
