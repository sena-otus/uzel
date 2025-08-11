#pragma once

//#include "msg.h"

#include <utility> // need to be before boost/asio.hpp
#include <boost/asio.hpp>
#include <unordered_map>
#include <vector>
#include <optional>
#include <functional>
#include <algorithm>
#include <cstdint>
#include <string>
#include <memory>

namespace uzel {

  // class Msg;
  //
  // template<class ShrSession>
  // class MsgDispatcher {
  // public:
  //   using StdMsgHandler = std::function<void(const Msg&)>;
  //   using SessionMsgHandler = std::function<void(const Msg&, ShrSession ss)>;

  //   void registerHandler(const std::string& cname, StdMsgHandler handler) {
  //     m_handlers[cname] = std::move(handler);
  //   }

  //   void registerHandler(StdMsgHandler handler);

  //   void registerHandler(const std::string& cname,  SessionMsgHandler handler) {
  //     m_handlers[cname] = std::move(handler);
  //   }

  //   void dispatch(const Msg& msg, ShrSession ss) const {
  //     auto it = m_handlers.find(msg.cname());
  //     if (it != m_handlers.end()) {
  //       std::visit([&](auto&& handler) {
  //         using T = std::decay_t<decltype(handler)>;
  //         if constexpr (std::is_same_v<T, StdMsgHandler>) {
  //           handler(msg);
  //         } else if constexpr (std::is_same_v<T, SessionMsgHandler>) {
  //           handler(msg,ss);
  //         }
  //       }, it->second);
  //     }
  //   }


  // private:
  //   std::unordered_map<std::string, std::variant<StdMsgHandler, SessionMsgHandler>> m_handlers;
  // };


  class Msg;

  class MsgDispatcher : public std::enable_shared_from_this<MsgDispatcher> {
  public:
    using shr_t   = std::shared_ptr<MsgDispatcher>;
    using Handler = std::function<void(const Msg&)>;
    using Id      = std::uint64_t;
    using IHPair  = std::pair<Id, Handler>;
    using HandlerVec     = std::vector<IHPair>;
    using HandlerVecPtr  = std::shared_ptr<const HandlerVec>;

    class Connection {
    public:
      Connection() = default;
      explicit Connection(std::function<void()> d) : m_disconnect(std::move(d)) {}
      Connection(Connection&& o) noexcept : m_disconnect(std::move(o.m_disconnect)) {}
      Connection& operator=(Connection&& o) noexcept {
        if (this != &o) { disconnect(); m_disconnect = std::move(o.m_disconnect); }
        return *this;
      }
      ~Connection() { disconnect(); }
      void disconnect() { if (m_disconnect) { m_disconnect(); m_disconnect = {}; } }
      Connection(const Connection&) = delete;
      Connection& operator=(const Connection&) = delete;
    private:
      std::function<void()> m_disconnect;
    };

    explicit MsgDispatcher(const boost::asio::any_io_executor& ex)
      : m_strand(boost::asio::make_strand(ex)) {}

      // Multicast: append a handler for specific cname.
    Connection registerHandler(const std::string& cname, Handler handler) {
      const auto id = m_nextId++;
      boost::asio::dispatch(m_strand, [this, cname, id, handler = std::move(handler)]() mutable {
        auto& entry = m_handlers[cname];
        HandlerVec newv = entry ? *entry : HandlerVec{};
        newv.emplace_back(id, std::move(handler));
          // small optimization: keep capacity growth amortized
        entry = std::make_shared<const HandlerVec>(std::move(newv));
      });
      return makeDisconnect(cname, id, Bucket::Cname);
    }

      // Any-post hook (append). Called after per-cname handlers.
    Connection registerAnyPost(Handler h) {
      const auto id = m_nextId++;
      boost::asio::dispatch(m_strand, [this, id, h = std::move(h)]() mutable {
        HandlerVec newv = m_anyPost ? *m_anyPost : HandlerVec{};
        newv.emplace_back(id, std::move(h));
        m_anyPost = std::make_shared<const HandlerVec>(std::move(newv));
      });
      return makeDisconnect(std::string{}, id, Bucket::AnyPost);
    }

      // Dispatch (no copies of handler vectors)
    void dispatch(std::shared_ptr<const Msg> msg);

      /** unregister all handlers for given cname */
    void unregisterAll(const std::string& cname) {
      boost::asio::dispatch(m_strand, [this, cname]{ m_handlers.erase(cname); });
    }
    void clearAnyPost() {
      boost::asio::dispatch(m_strand, [this]{ m_anyPost.reset(); });
    }

    boost::asio::any_io_executor get_executor() const noexcept {
      return m_strand.get_inner_executor();
    }

  private:
    enum class Bucket { Cname, AnyPost };

    Connection makeDisconnect(std::string cname, Id id, Bucket bucket);
      // members
    boost::asio::strand<boost::asio::any_io_executor> m_strand;
      // cname -> snapshot of handlers
    std::unordered_map<std::string, HandlerVecPtr> m_handlers;
      // any-post snapshot
    HandlerVecPtr m_anyPost;
    Id m_nextId{1};
  };
}
