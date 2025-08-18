#pragma once

#include <utility> // need to be before boost/asio.hpp
#include <boost/asio.hpp>
#include <unordered_map>
#include <vector>
#include <functional>
#include <cstdint>
#include <string>
#include <memory>

namespace uzel {
  class Msg;
  using ShrMsg = std::shared_ptr<Msg>;
  using ShrConstMsg = std::shared_ptr<const Msg>;

  class MsgDispatcher : public std::enable_shared_from_this<MsgDispatcher> {
  public:
    using shr_t   = std::shared_ptr<MsgDispatcher>;
    using Id      = std::uint64_t;
    using Handler = std::function<void(const Msg&)>;
    using IHPair  = std::pair<Id, Handler>;
    using HandlerVec     = std::vector<IHPair>;
    using HandlerVecPtr  = std::shared_ptr<const HandlerVec>;
    using HandlerShr = std::function<void(ShrMsg)>;
    using IHSPair  = std::pair<Id, HandlerShr>;
    using HandlerShrVec     = std::vector<IHSPair>;
    using HandlerShrVecPtr  = std::shared_ptr<const HandlerShrVec>;

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

    explicit MsgDispatcher(const boost::asio::any_io_executor& ex);

    void assertShared() const;

      // Multicast: append a handler for specific cname.
    Connection registerHandler(const std::string& cname, Handler handler);

      // Any-post hook (append). Called after per-cname handlers.
    Connection registerAnyPost(HandlerShr handler);

      // Dispatch
    void dispatch(ShrMsg msg);

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
    HandlerShrVecPtr m_anyPost;
    Id m_nextId{1};
  };
}
