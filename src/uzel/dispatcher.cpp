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


  MsgDispatcher::Connection MsgDispatcher::registerHandler(const std::string& cname, Handler handler)
  {
    assertShared();
    const auto id = m_nextId++;
    boost::asio::dispatch(m_strand, [this, cname, id, handler = std::move(handler)]() mutable {
      auto& entry = m_handlers[cname];
      HandlerVec newv = entry ? *entry : HandlerVec{};
      newv.emplace_back(id, std::move(handler));
        // small optimization: keep capacity growth amortized
      entry = std::make_shared<const HandlerVec>(std::move(newv));
    });
    return makeDisconnect(cname, id);
  }


  MsgDispatcher::Connection MsgDispatcher::registerAnyPost(HandlerShr handler)
  {
    assertShared();
    const auto id = m_nextId++;
    boost::asio::dispatch(m_strand, [this, id, h = std::move(handler)]() mutable {
      HandlerShrVec newv = m_anyPost ? *m_anyPost : HandlerShrVec{};
      newv.emplace_back(id, std::move(h));
      m_anyPost = std::make_shared<const HandlerShrVec>(std::move(newv));
    });
    return makeDisconnect(std::string{}, id);
  }


  void MsgDispatcher::dispatch(Msg::shr_t msg)
  {
    assertShared();

      // capture snapshot pointers (cheap) and iterate
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
        if (per)  for (const auto& kv : *per)  kv.second(*msg);
      }
      HandlerShrVecPtr post = self->m_anyPost;
      if (post) for (const auto& kv : *post) kv.second(msg);
    });
  }


  MsgDispatcher::Connection MsgDispatcher::makeDisconnect(std::string cname, Id id)
  {
    std::weak_ptr<MsgDispatcher> wself = this->shared_from_this();
    return Connection([wself, cname = std::move(cname), id]{
      if (auto self = wself.lock()) {
        boost::asio::dispatch(self->m_strand, [self, cname, id]{
          if (!cname.empty()) {
            auto it = self->m_handlers.find(cname);
            if (it == self->m_handlers.end() || !it->second) return;
            HandlerVec newhv = *it->second;
            newhv.erase(std::remove_if(newhv.begin(), newhv.end(),
                                       [&](auto& kv){ return kv.first == id; }),
                        newhv.end());
            if (newhv.empty()) self->m_handlers.erase(it);
            else it->second = std::make_shared<const HandlerVec>(std::move(newhv));
          } else {
            if (!self->m_anyPost) return;
            HandlerShrVec newhv = *self->m_anyPost;
            newhv.erase(std::remove_if(newhv.begin(), newhv.end(),
                                       [&](auto& kv){ return kv.first == id; }),
                        newhv.end());
            if (newhv.empty()) self->m_anyPost.reset();
            else self->m_anyPost = std::make_shared<const HandlerShrVec>(std::move(newhv));
          }
        });
      }
    });
  }
}
