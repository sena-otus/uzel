#include "OutgoingManager.h"
#include "remote.h"

#include <boost/log/trivial.hpp>
#include <stdexcept>
#include <uzel/uconfig.h>
#include <uzel/dbg.h>
#include <uzel/session.h>

using boost::asio::ip::tcp;
namespace sys = boost::system;

namespace uzel {

  OutgoingManager::OutgoingManager(
    NetAppContext::shr_t netctx,
    const uzel::IpToSession &ipToSession,
    const uzel::NodeToSession &nodeToSession,
    unsigned port, unsigned wantedConnections)
    : m_netctx(std::move(netctx)), m_strand(m_netctx->iocontext().get_executor()), m_timer(m_strand),
      m_ipToSession(ipToSession), m_nodeToSession(nodeToSession),
      m_port(port),
      m_wantedConnections(wantedConnections)
  {
  }

  void OutgoingManager::startConnecting(const std::string &hname) {
    if(uzel::UConfigS::getUConfig().isLocalNode(hname)) {
      return;
    }
    m_connectTo.emplace(hname, std::make_shared<RemoteHostToConnect>(hname, std::to_string(m_port), m_wantedConnections));
  }


  void OutgoingManager::start() {
    scheduleNext(0);
  }

  void OutgoingManager::poke() {
    boost::asio::post(m_strand, [this]{
        // affects the existing async_wait
      m_timer.expires_after(std::chrono::seconds(0));
    });
  }


  void OutgoingManager::scheduleNext(int seconds) {
    m_timer.expires_after(std::chrono::seconds(seconds));
    m_timer.async_wait(
      boost::asio::bind_executor(
        m_strand,
        [this](const boost::system::error_code& ec) {
          if (ec) {
              // timer is canceled
            return;
          }
          startConnecting(); // do actual work
          scheduleNext(RefreshHostStatus_sec); // re-arm regular cadence
        }));
  }


  void OutgoingManager::startConnecting()
  {
    auto now = std::chrono::steady_clock::now();

    for(auto && hit : m_connectTo) {
      switch(hit.second->status())
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
          if(std::chrono::duration_cast<std::chrono::seconds>(now - hit.second->statusChangeTS()).count() > DelayReconnect_sec) {
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

  void OutgoingManager::startResolving(RemoteHostToConnect::shr_t rh)
  {
    rh->setStatus(HostStatus::resolving);
    BOOST_LOG_TRIVIAL(debug) << DBGOUT << " resolving " << rh->hostname() << "...";
    m_netctx->aresolver().async_resolve<tcp>(rh->hostname(), rh->service(),
                                             [this, rh](const sys::error_code ec, const tcp::resolver::results_type resit){
                                               connectResolved(ec,resit,rh);
                                             });
  }


  void OutgoingManager::connectResolved(const sys::error_code ec, const tcp::resolver::results_type rezit, RemoteHostToConnect::shr_t rh)
  {
    if(ec) {
      BOOST_LOG_TRIVIAL(error) << "error resolving '" << rh->hostname() << "': "<<  ec.message();
      rh->setStatus(HostStatus::resolving_error);
        // poke, so we will try to reconnect, if necessary
      poke();
      return;
    }

    BOOST_LOG_TRIVIAL(info) << "resolved "  << rezit->host_name() << "->" << rezit->endpoint();

    rh->setAddr(rezit->endpoint().address());


      // Logic for wantedConnections == 1:
      // If there is 1+ outgoing authenticated session to that IP, then stop
      // If there is 1 outgoing unauthenticated session to that IP, then wait
      //
      // Logic for wantedConnections == 2:
      // If we have 1 outgoing authenticated session to that IP and
      // another authenticated session with any IP, incoming or outgoing,
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
      //
      // Possible optimizations:
      // 1. Now we create 2 outgoing connections at the same time (if
      // there are no authenticated connections to that IP). That could
      // be optimized:
      // first create single connection, authenticate and wait several
      // seconds, because remote will connect back, if possible.
      // 2. For that to happen, connect back after accepting and
      // authentication of the remote node (resolve remote node name and
      // connect to that IP).


    size_t sessions{0};
    size_t unauthsessions{0};
    size_t sessionsToCreate{0};

    auto ipit = m_ipToSession.find(rezit->endpoint().address());

    if(ipit == m_ipToSession.end()|| ipit->second.empty()) {
      BOOST_LOG_TRIVIAL(debug) << "there are no sessions for IP '"<< rezit->endpoint()
                               << ", will create "<< rh->wantedConnections()<< " connections";
      sessionsToCreate = rh->wantedConnections();
    } else {
      for(auto && sit : ipit->second) {
        BOOST_LOG_TRIVIAL(debug) << "found session "<< sit << " for IP '"<< rezit->endpoint();
        if(sit->direction() == +uzel::Direction::outgoing) {
          BOOST_LOG_TRIVIAL(debug) << "it is outgoing";
          ++sessions;
          if(!sit->authenticated()) {
            ++unauthsessions;
          } else {
            BOOST_LOG_TRIVIAL(debug) << "it is authenticated as " << sit->peerNode();
            auto nodeit = m_nodeToSession.find(sit->peerNode());
            if(nodeit != m_nodeToSession.end())
            {
              if(nodeit->second.connected())
              {
                BOOST_LOG_TRIVIAL(debug) << "there are already enough authenticated sessions with '" << sit->peerNode()
                                         << "', so do not create a new one, set status to connected";
                if(rh->status() != +HostStatus::connected) { rh->setStatus(HostStatus::connected); }
                return;
              }
            } else {
              BOOST_LOG_TRIVIAL(error) << "that can never happen: authenticated session is immediately added to n_nodeToSession!";
            }
          }
        }   else {
          BOOST_LOG_TRIVIAL(debug) << "it is incoming - skip";
        }
      }
        // here we know, that we are not done with connecting
      if(sessions >= rh->wantedConnections()) {
        if(unauthsessions > 0) {
            // wait for connection and authentication
          if(rh->status() != +HostStatus::connecting) { rh->setStatus(HostStatus::connecting); }
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
        sessionsToCreate = rh->wantedConnections() - sessions;
      }
    }

    BOOST_LOG_TRIVIAL(debug) << "Before creation: outgoing sessions: " << sessions
                             << ", unauthenticated: "  << unauthsessions
                             << ", sessions to create: " << sessionsToCreate;

    rh->setStatus(HostStatus::connecting);
    while(sessionsToCreate != 0)
    {
      tcp::socket sock{m_netctx->iocontext()};
      auto unauth = std::make_shared<uzel::session>(m_netctx, std::move(sock), uzel::Direction::outgoing, rezit->endpoint().address(), rh->hostname());
      BOOST_LOG_TRIVIAL(debug) << "session '"<<unauth << "' created outgoing";
      s_sessionCreated(unauth);

      unauth->s_closed.connect([&,rh]() {
        rh->setStatus(HostStatus::closed);
          // poke, so we will try to reconnect if necessary
        poke();
      });

        // follwoing not needed, as session::s_closed() will be fired:
        // unauth->s_connect_error.connect([&](const std::string &hname){
        //   rh.setStatus(HostStatus::connection_error);
        // });

      unauth->startConnection(rezit);
      sessions++;
      unauthsessions++;
      sessionsToCreate--;
    }
    rh->setStatus(HostStatus::connecting);
    BOOST_LOG_TRIVIAL(debug) << "Exiting connectResolved with outgoing sessions: " << sessions
                             << ", unauthenticated: "  << unauthsessions;
  }

}
