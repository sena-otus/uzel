#include "netapp.h"
#include "session.h"

using boost::asio::ip::tcp;

NetApp::NetApp(boost::asio::io_context& io_context, unsigned short port)
  : m_acceptor(io_context, tcp::endpoint(tcp::v4(), port))
{
  do_accept();
}


void NetApp::do_accept()
{
  m_acceptor.async_accept(
    [this](boost::system::error_code ec, tcp::socket socket)
      {
        if (!ec)
        {
          auto unauth = std::make_shared<session>(std::move(socket));
          unauth->s_auth.connect([&](session::shr_t ss){ auth(ss); });
          unauth->s_dispatch.connect([&](uzel::Msg &msg){ dispatch(msg);});
          unauth->start();
        }

        do_accept();
      });
}

void NetApp::localMsg(uzel::Msg & msg)
{
  auto lit = m_locals.find(msg.dest().app());
  if(lit != m_locals.end()) {
    lit->second->putOutQueue(msg);
  }
}

void NetApp::localbroadcastMsg(uzel::Msg & msg)
{
  std::for_each(m_locals.begin(), m_locals.end(),
                [&msg](auto &sp){
                  sp.second->putOutQueue(msg);
                });

}


void NetApp::broadcastMsg(uzel::Msg & msg)
{
  std::for_each(m_remotes.begin(), m_remotes.end(),
                [&msg](auto &sp){
                  sp.second->putOutQueue(msg);
                });
}


void NetApp::remoteMsg(uzel::Msg & msg)
{
  auto rit = m_remotes.find(msg.dest().node());
  if(rit != m_remotes.end())
  {
    rit->second->putOutQueue(msg);
  }
}

void NetApp::dispatch(uzel::Msg &msg)
{
  switch(msg.destType())
  {
    case uzel::Msg::DestType::local:
      localMsg(msg);
      break;
    case uzel::Msg::DestType::remote:
      remoteMsg(msg);
      break;
    case uzel::Msg::DestType::broadcast:
      broadcastMsg(msg);
      break;
    case uzel::Msg::DestType::localbroadcast:
      localbroadcastMsg(msg);
      break;
  }
}

void NetApp::auth(session::shr_t ss)
{
  if(ss->msg1().isLocal()) {
    m_locals.emplace(ss->msg1().from().app(), ss);
  } else {
    m_remotes.emplace(ss->msg1().from().node(), ss);
  }
}
