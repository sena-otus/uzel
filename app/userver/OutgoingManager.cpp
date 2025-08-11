#include "OutgoingManager.h"
#include "remote.h"

#include <boost/log/trivial.hpp>
#include <uzel/uconfig.h>
#include <uzel/dbg.h>
#include <uzel/session.h>

using boost::asio::ip::tcp;
namespace io = boost::asio;
namespace sys = boost::system;

namespace uzel {

OutgoingManager::OutgoingManager(
  NetAppContext::shr_t netctx,
  const uzel::IpToSession &ipToSession,
  const uzel::NodeToSession &nodeToSession)
  : m_netctx(netctx), m_ipToSession(ipToSession), m_nodeToSession(nodeToSession)
{
}

void OutgoingManager::startConnecting(const std::string &hname) {
  if(uzel::UConfigS::getUConfig().isLocalNode(hname)) {
    return;
  }
  m_connectTo.emplace(hname, RemoteHostToConnect(hname));
}


void OutgoingManager::startConnecting() {
  auto timer = std::make_shared<io::steady_timer>(m_netctx->iocontext(), io::chrono::seconds(RefreshHostStatus_sec));
  startConnecting(timer);
}

void OutgoingManager::startConnecting(std::shared_ptr<io::steady_timer> timer)
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
      case +HostStatus::connected:
          // do nothing
        break;
    }
  }
}

void OutgoingManager::startResolving(RemoteHostToConnect &rh)
{
  rh.setStatus(HostStatus::resolving);
  BOOST_LOG_TRIVIAL(debug) << DBGOUT << " resolving " << rh.hostname() << "...";
  m_netctx->aresolver().async_resolve<tcp>(rh.hostname(), "32300",
                                 [this,&rh](const sys::error_code ec, const tcp::resolver::results_type resit){
                                   BOOST_LOG_TRIVIAL(debug) << DBGOUT;
                                   connectResolved(ec,resit,rh);
                                 });
}


void OutgoingManager::connectResolved(const sys::error_code ec, const tcp::resolver::results_type rezit, RemoteHostToConnect &rh)
{
  if(ec) {
    BOOST_LOG_TRIVIAL(error) << "error resolving '" << rh.hostname() << "': "<<  ec.message();
    rh.setStatus(HostStatus::resolving_error);
    return;
  }

  BOOST_LOG_TRIVIAL(info) << "resolved "  << rezit->host_name() << "->" << rezit->endpoint();

  auto ipit = m_ipToSession.find(rezit->endpoint().address());

    // If we have already 2 outgoing sessions to that IP, then stop.
    // Do not count incoming connections from the same IP, they may be
    // not from the same host because of the NAT, redirections, etc.
    //
    // But if we have 1 outgoing authenticated session to that IP and
    // another authenticated session with the same node, then we should
    // stop too

  size_t sessions = 0;
  std::string outgoingnode;
  if(ipit != m_ipToSession.end()) {
    for(auto && sit : ipit->second) {
      if(sit->direction() == +uzel::Direction::outgoing) {
        ++sessions;
        if(sessions >= 2) {
          if(rh.status() != +HostStatus::connected) { rh.setStatus(HostStatus::connected); }

          BOOST_LOG_TRIVIAL(debug) << "there are already "  << sessions
                                   << " outgoing sessions, so do not create a new one, set status to connected";
          return;
        }
        if(sit->authenticated()) {
            // remember the node name
          outgoingnode = sit->remoteNode();
        }
      }
    }
    if(!outgoingnode.empty()) {
        // now we should check if we have 2 authenticated sessions with that node
      auto nodeit = m_nodeToSession.find(outgoingnode);
      if(nodeit != m_nodeToSession.end())
      {
        if(nodeit->second.sessionCount() >= 2)
        {
          if(rh.status() != +HostStatus::connected) { rh.setStatus(HostStatus::connected); }

          BOOST_LOG_TRIVIAL(debug) << "there are already "  << sessions
                                   << " authenticated sessions, so do not create a new one, set status to connected";
          return;
        }
      }
    }
  }

  rh.setAddr(rezit->endpoint().address());
  rh.setStatus(HostStatus::connecting);
  while(sessions < 2)
  {
    tcp::socket sock{m_netctx->iocontext()};
    auto unauth = std::make_shared<uzel::session>(m_netctx, std::move(sock), uzel::Direction::outgoing, rezit->endpoint().address(), rh.hostname());
    s_sessionCreated(unauth);

    unauth->s_closed.connect([&](uzel::session::shr_t ss) {
      rh.setStatus(HostStatus::closed);
    });

    unauth->s_connect_error.connect([&](const std::string &hname){
      rh.setStatus(HostStatus::connection_error);
    });
    unauth->startConnection(rezit);
    sessions++;
  }
  rh.setStatus(HostStatus::connected);
}

}
