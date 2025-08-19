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

      // for named handler, with const Msg&
    using Handler = std::function<void(const Msg&)>;
    using IHPair  = std::pair<Id, Handler>;
    using HandlerVec     = std::vector<IHPair>;
    using HandlerVecPtr  = std::shared_ptr<const HandlerVec>;
      // for any handler, with ShrMsg
    using HandlerShr = std::function<void(ShrMsg)>;
    using IHSPair  = std::pair<Id, HandlerShr>;
    using HandlerShrVec     = std::vector<IHSPair>;
    using HandlerShrVecPtr  = std::shared_ptr<const HandlerShrVec>;



    class Connection {
    public:
      Connection() = default;

      void disconnect() {
        if (auto self = m_self.lock()) {
          self->disconnectImpl(m_cname, m_id);
        }
          // make idempotent
        m_self.reset();
        m_id = 0;
      }

      [[nodiscard]] bool connected() const noexcept { return !m_self.expired() && m_id != 0; }

    private:
      friend class MsgDispatcher;

      Connection(std::weak_ptr<MsgDispatcher> self,
                 std::string cname, Id id)
        : m_self(std::move(self)), m_cname(std::move(cname)), m_id(id) {}

      std::weak_ptr<MsgDispatcher> m_self;
      std::string m_cname;
      Id m_id{0};
    };


    class ScopedConnection {
    public:
      ScopedConnection() = default;
      explicit ScopedConnection(Connection c) : m_c(std::move(c)) {}
      ~ScopedConnection() { m_c.disconnect(); }
      ScopedConnection(ScopedConnection&&) = default;
      ScopedConnection& operator=(ScopedConnection&&) = default;
      ScopedConnection(const ScopedConnection&) = delete;
      ScopedConnection& operator=(const ScopedConnection&) = delete;
    private:
      Connection m_c;
    };

    explicit MsgDispatcher(const boost::asio::any_io_executor& ex);

    void assertShared() const;

      /**
       * Multicast: append a handler for specific cname.
       * @return Connection object that can be used to disconnect
       * */
    Connection registerHandler(const std::string& cname, Handler handler);

      /**
       * Multicast: append a handler for specific cname.
       * @return ScopedConnection object that will disconnect on destruction (RAII)
       * */
    ScopedConnection registerHandlerScoped(const std::string& cname, Handler handler);

      // Any-post hook (append). Called after per-cname handlers.
    Connection registerAnyPost(HandlerShr handler);

      // Any-post hook (append). Called after per-cname handlers.
    ScopedConnection registerAnyPostScoped(HandlerShr handler);

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
    Id nextIdOnStrand();
    void disconnectImpl(const std::string& cname, Id id);

        // members
    boost::asio::strand<boost::asio::any_io_executor> m_strand;
      // cname -> snapshot of handlers
    std::unordered_map<std::string, HandlerVecPtr> m_handlers;
      // any-post snapshot
    HandlerShrVecPtr m_anyPost;
    Id m_nextId{1};
  };

}
