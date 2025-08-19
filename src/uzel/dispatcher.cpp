#include "dispatcher.h"
#include "msg.h"
#include <boost/log/trivial.hpp>
#include <memory>

namespace uzel {

  MsgDispatcher::MsgDispatcher(const boost::asio::any_io_executor& ex)
    : m_strand(boost::asio::make_strand(ex))
  {
  }

  void MsgDispatcher::assertShared() const
  {
#ifndef NDEBUG
    if (weak_from_this().use_count() == 0) {
      throw std::logic_error("MsgDispatcher must be owned by std::shared_ptr");
    }
#endif
  }

  MsgDispatcher::Id MsgDispatcher::nextIdOnStrand() {
    Id id;
    boost::asio::dispatch(m_strand, [this, &id]{ id = m_nextId++; });
    return id;
  }


  MsgDispatcher::Connection MsgDispatcher::registerHandler(const std::string& cname, Handler handler)
  {
    assertShared();
    const auto id = nextIdOnStrand();
    boost::asio::dispatch(m_strand, [this, id, cname, handler = std::move(handler)]() mutable {
      auto& entry = m_handlers[cname];
      HandlerVec newv = entry ? *entry : HandlerVec{};
        // do not modify original vector, but create a new shared_ptr<HandlerVec>,
        // so that we can also register/unregister on the fly from any handler without
        // a danger to invalidate the iterator in dispatch() loop
      newv.emplace_back(id, std::move(handler));
      entry = std::make_shared<const HandlerVec>(std::move(newv));
    });
    return {weak_from_this(), cname, id};
  }

  MsgDispatcher::ScopedConnection MsgDispatcher::registerHandlerScoped(const std::string& cname, Handler handler)
  {
    return ScopedConnection(registerHandler(cname, handler));
  }

  MsgDispatcher::Connection MsgDispatcher::registerAnyPost(HandlerShr handler)
  {
    assertShared();
    const auto id = nextIdOnStrand();
    boost::asio::dispatch(m_strand, [this, id, h = std::move(handler)]() mutable {
      HandlerShrVec newv = m_anyPost ? *m_anyPost : HandlerShrVec{};
      newv.emplace_back(id, std::move(h));
      m_anyPost = std::make_shared<const HandlerShrVec>(std::move(newv));
    });
    return {weak_from_this(), std::string{}, id};
  }

  MsgDispatcher::ScopedConnection MsgDispatcher::registerAnyPostScoped(HandlerShr handler)
  {
    return ScopedConnection(registerAnyPost(handler));
  }

  void MsgDispatcher::dispatch(Msg::shr_t msg)
  {
    assertShared();

      // capture snapshot pointers (instead of copying) and iterate, see also
      // explanation in registerHandler()
    boost::asio::dispatch(m_strand, [self = shared_from_this(), msg = std::move(msg)]{
      const auto cname = msg->cname();

      BOOST_LOG_TRIVIAL(debug) << "dispatching cname '" << cname << "'";

        // dispatch to cname handlers only if message is to myself
      if(msg->toMe())
      {
        HandlerVecPtr per = {};
        if (auto it = self->m_handlers.find(cname); it != self->m_handlers.end()) {
          per = it->second;
        }
        if (per)  for (const auto& kv : *per)  {
            try {
              kv.second(*msg);
            }
            catch (const std::exception& e) { BOOST_LOG_TRIVIAL(error) << "handler threw: " << e.what(); }
            catch (...) { BOOST_LOG_TRIVIAL(error) << "handler threw unknown"; }
          }
      }
      HandlerShrVecPtr post = self->m_anyPost;
      if (post) for (const auto& kv : *post) {
          try {
            kv.second(msg);
          }
          catch (const std::exception& e) { BOOST_LOG_TRIVIAL(error) << "post handler threw: " << e.what(); }
          catch (...) { BOOST_LOG_TRIVIAL(error) << "post handler threw unknown"; }
        }
    });
  }

  void MsgDispatcher::disconnectImpl(const std::string& cname, Id id)
  {
    boost::asio::dispatch(m_strand, [this, cname, id] {
      if(!cname.empty())
      {
        auto it = m_handlers.find(cname);
        if (it == m_handlers.end() || !it->second) {
          return;
        }
        HandlerVec nv = *it->second;
        nv.erase(std::remove_if(nv.begin(), nv.end(),
                                [&](auto& item){ return item.first == id; }),
                 nv.end());
        if (nv.empty()) m_handlers.erase(it);
        else it->second = std::make_shared<const HandlerVec>(std::move(nv));
      } else {
          // empty cname -> any handler
        if (!m_anyPost) return;
        HandlerShrVec nv = *m_anyPost;
        nv.erase(std::remove_if(nv.begin(), nv.end(),
                                [&](auto& item){ return item.first == id; }),
                 nv.end());
        if (nv.empty()) {
          m_anyPost.reset();
        } else {
          m_anyPost = std::make_shared<const HandlerShrVec>(std::move(nv));
        }
      }
    });
  }

}
