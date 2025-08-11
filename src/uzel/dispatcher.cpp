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
    return makeDisconnect(cname, id, Bucket::Cname);
  }


  MsgDispatcher::Connection MsgDispatcher::registerAnyPost(Handler h)
  {
    assertShared();
    const auto id = m_nextId++;
    boost::asio::dispatch(m_strand, [this, id, h = std::move(h)]() mutable {
      HandlerVec newv = m_anyPost ? *m_anyPost : HandlerVec{};
      newv.emplace_back(id, std::move(h));
      m_anyPost = std::make_shared<const HandlerVec>(std::move(newv));
    });
    return makeDisconnect(std::string{}, id, Bucket::AnyPost);
  }


  void MsgDispatcher::dispatch(Msg::shr_const_t msg)
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
      HandlerVecPtr post = self->m_anyPost;
      if (post) for (const auto& kv : *post) kv.second(*msg);
    });
  }


  MsgDispatcher::Connection MsgDispatcher::makeDisconnect(std::string cname, Id id, Bucket bucket)
  {
    std::weak_ptr<MsgDispatcher> wself = this->shared_from_this();
    return Connection([wself, cname = std::move(cname), id, bucket]{
      if (auto self = wself.lock()) {
        boost::asio::dispatch(self->m_strand, [self, cname, id, bucket]{
          if (bucket == Bucket::Cname) {
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
            HandlerVec newhv = *self->m_anyPost;
            newhv.erase(std::remove_if(newhv.begin(), newhv.end(),
                                       [&](auto& kv){ return kv.first == id; }),
                        newhv.end());
            if (newhv.empty()) self->m_anyPost.reset();
            else self->m_anyPost = std::make_shared<const HandlerVec>(std::move(newhv));
          }
        });
      }
    });
  }
}
