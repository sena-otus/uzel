#include "ConnectionManager.h"

#include <uzel/uconfig.h>
#include <uzel/dbg.h>
#include <uzel/session.h>

using boost::asio::ip::tcp;
namespace io = boost::asio;
namespace sys = boost::system;


ConnectionManager::ConnectionManager(boost::asio::io_context& iocontext, aresolver &resolver)
  : m_iocontext(iocontext), m_aresolver(resolver)
{
}

void ConnectionManager::startConnecting(const std::string &hname) {
  if(uzel::UConfigS::getUConfig().isLocalNode(hname)) {
    return;
  }
  m_connectTo.emplace(hname, RemoteHostToConnect(hname));
}


void ConnectionManager::startConnecting() {
  auto timer = std::make_shared<io::steady_timer>(m_iocontext, io::chrono::seconds(RefreshHostStatus_sec));
  startConnecting(timer);
}

void ConnectionManager::startConnecting(std::shared_ptr<io::steady_timer> timer)
{
  auto now = std::chrono::steady_clock::now();

  timer->expires_after(boost::asio::chrono::seconds(RefreshHostStatus_sec));
  timer->async_wait([&,timer](const sys::error_code&  /*ec*/){
    startConnecting(timer);
  });


  for(auto && hit : m_connectTo) {
    switch(hit.second.status())
    {
      case +HostStatus::initial:
      {
        startResolving(hit.second);
      }
      break;
      case +HostStatus::connection_error: [[fallthrough]] ;
      case +HostStatus::resolving_error:
      case +HostStatus::closed:
      {
        if(std::chrono::duration_cast<std::chrono::seconds>(now - hit.second.statusChangeTS()).count() > DelayReconnect_sec) {
          startResolving(hit.second);
        }
      }
      break;
      case +HostStatus::connecting:  [[fallthrough]] ;
      case +HostStatus::resolving:
      case +HostStatus::authenticated:
          // do nothing
        break;
    }
  }
}

void ConnectionManager::startResolving(RemoteHostToConnect &rh)
{
  rh.setStatus(HostStatus::resolving);
  BOOST_LOG_TRIVIAL(debug) << DBGOUT << " resolving " << rh.hostname() << "...";
  m_aresolver.async_resolve<tcp>(rh.hostname(), "32300",
                                 [this,&rh](const sys::error_code ec, const tcp::resolver::results_type resit){
                                   BOOST_LOG_TRIVIAL(debug) << DBGOUT;
                                   connectResolved(ec,resit,rh);
                                 });
}


void ConnectionManager::connectResolved(const sys::error_code ec, const tcp::resolver::results_type rezit, RemoteHostToConnect &rh)
{
  if(ec) {
    BOOST_LOG_TRIVIAL(error) << "error resolving '" << rh.hostname() << "': "<<  ec.message();
    rh.setStatus(HostStatus::resolving_error);
  } else {
    BOOST_LOG_TRIVIAL(info) << "resolved "  << rezit->host_name() << "->" << rezit->endpoint() << ", connecting...";
    {
      rh.setAddr(rezit->endpoint().address());
      rh.setStatus(HostStatus::connecting);
      tcp::socket sock{m_iocontext};
      auto unauth = std::make_shared<uzel::session>(std::move(sock), uzel::Direction::outgoing, rezit->endpoint().address(), hname);
      m_connectionsPerAddr[rezit->endpoint().address()].insert(unauth);
      unauth->s_closed.connect([&](uzel::session::shr_t ss){
        rh.setStatus(HostStatus::closed);
        onSessionClosed(ss);});
      unauth->s_connect_error.connect([&](const std::string &hname){ rh.setStatus(HostStatus::connection_error); });
      unauth->s_auth.connect([&](uzel::session::shr_t ss) {
        rh.setStatus(HostStatus::authenticated);
        auth(ss); });
      unauth->s_dispatch.connect([&](uzel::Msg &msg){ dispatch(msg);});
      unauth->startConnection(rezit);
    }
  }
}
