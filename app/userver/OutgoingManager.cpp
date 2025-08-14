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
    : m_netctx(std::move(netctx)), m_ipToSession(ipToSession), m_nodeToSession(nodeToSession)
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
        case +HostStatus::connecting:  [[fallthrough]] ; // check timeout?
        case +HostStatus::resolving:  [[fallthrough]] ; // check timeout?
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

    rh.setAddr(rezit->endpoint().address());


      // If we have 1 outgoing authenticated session to that IP and
      // another authenticated session (with any IP, incoming or outgoing)
      // with the same remote node, then do not create one more connection,
      // mark that remote name as "connected" and return.
      //
      // else if we have already 2+ outgoing sessions to that IP and one or
      // more are not authenticated, then do not create one more
      // connection, mark that remote name as "connecting" and return.
      //
      // else create 2 outgoing sessions to that IP.
      //
      // Note: even if we have 2 incoming authenticated connections from
      // that IP, we need at least one outgoing to be sure node name is
      // the same.
      //
      // Recomendation: remote nodes names that are resolvable to their IP
      // make life simpler.


    size_t sessions{0};
    size_t unauthsessions{0};
    size_t sessionsToCreate{0};

    auto ipit = m_ipToSession.find(rezit->endpoint().address());
    if(ipit == m_ipToSession.end()) {
      sessionsToCreate = 2;
    } else {
      for(auto && sit : ipit->second) {
        if(sit->direction() == +uzel::Direction::outgoing) {
          ++sessions;
          if(!sit->authenticated()) {
            ++unauthsessions;
          } else {
            auto nodeit = m_nodeToSession.find(sit->peerNode());
            if(nodeit != m_nodeToSession.end())
            {
              if(nodeit->second.sessionCount() >= 2)
              {
                BOOST_LOG_TRIVIAL(debug) << "there are already "  << nodeit->second.sessionCount()
                                         << " authenticated sessions with '" << sit->peerNode()
                                         << "', so do not create a new one, set status to connected";
                if(rh.status() != +HostStatus::connected) { rh.setStatus(HostStatus::connected); }
                return;
              }
            }
          }
        }
      }
        // here we know, that we are not done with connecting
      if(sessions >= 2) {
        if(unauthsessions > 0) {
            // wait for connection and authentication
          if(rh.status() != +HostStatus::connecting) { rh.setStatus(HostStatus::connecting); }
          BOOST_LOG_TRIVIAL(debug) << "there are "  << sessions
                                   << " outgoing sessions, but " << unauthsessions
                                   << " are not authenticated, wait for authentication";
          return;
        }
          // We have more than one outgoing authenticated sessions to
          // that IP all with different node names
          // How is that possible?
          // * node name was changed and connection to old name is still
          //   not closed: just wait until the old connection will be closed
          //   or create one more connection
          // * some special routing/forwarding/proxying magic in action:
          //   create one more connection
        sessionsToCreate = 1;
      } else {
        sessionsToCreate = 2 - sessions;
      }
    }

    BOOST_LOG_TRIVIAL(debug) << "Before creation: outgoing sessions: " << sessions
                             << ", unauthenticated: "  << unauthsessions
                             << ", sessions to create: " << sessionsToCreate;

    rh.setStatus(HostStatus::connecting);
    while(sessionsToCreate != 0)
    {
      tcp::socket sock{m_netctx->iocontext()};
      auto unauth = std::make_shared<uzel::session>(m_netctx, std::move(sock), uzel::Direction::outgoing, rezit->endpoint().address(), rh.hostname());
      BOOST_LOG_TRIVIAL(debug) << "session '"<<unauth << "' created outgoing";
      s_sessionCreated(unauth);

      unauth->s_closed.connect([&](uzel::session::shr_t ss) {
        rh.setStatus(HostStatus::closed);
      });

      unauth->s_connect_error.connect([&](const std::string &hname){
        rh.setStatus(HostStatus::connection_error);
      });
      unauth->startConnection(rezit);
      sessions++;
      unauthsessions++;
      sessionsToCreate--;
    }
    rh.setStatus(HostStatus::connecting);
    BOOST_LOG_TRIVIAL(debug) << "Exiting connectResolved with outgoing sessions: " << sessions
                             << ", unauthenticated: "  << unauthsessions;
  }

}
