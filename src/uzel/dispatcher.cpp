#include "dispatcher.h"
#include "msg.h"

namespace uzel {

  void MsgDispatcher::dispatch(Msg::shr_const_t msg)
  {
      // capture snapshot pointers (cheap) and iterate
    boost::asio::dispatch(m_strand, [self = shared_from_this(), msg = std::move(msg)]{
      const auto cname = msg->cname();

      HandlerVecPtr per = {};
      if (auto it = self->m_handlers.find(cname); it != self->m_handlers.end()) {
        per = it->second;
      }

      HandlerVecPtr post = self->m_anyPost;

        // dispatch to cname handlers only if message is to myself
      if(msg->toMe())
      {
        if (per)  for (const auto& kv : *per)  kv.second(*msg);
      }
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
