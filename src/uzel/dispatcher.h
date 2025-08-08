#pragma once

#include "msg.h"

namespace uzel {

  template<class ShrSession>
  class MsgDispatcher {
  public:
    using StdMsgHandler = std::function<void(const Msg&)>;
    using SessionMsgHandler = std::function<void(const Msg&, ShrSession ss)>;

    void registerHandler(const std::string& cname, StdMsgHandler handler) {
      m_serviceHandlers[cname] = std::move(handler);
    }

    void registerHandler(const std::string& cname,  SessionMsgHandler handler) {
      m_serviceHandlers[cname] = std::move(handler);
    }

    void dispatch(const Msg& msg, ShrSession ss) const {
      auto it = m_serviceHandlers.find(msg.cname());
      if (it != m_serviceHandlers.end()) {
        std::visit([&](auto&& handler) {
          using T = std::decay_t<decltype(handler)>;
          if constexpr (std::is_same_v<T, StdMsgHandler>) {
            handler(msg);
          } else if constexpr (std::is_same_v<T, SessionMsgHandler>) {
            handler(msg,ss);
          }
        }, it->second);
      }
    }

  private:
    std::unordered_map<std::string, std::variant<StdMsgHandler, SessionMsgHandler>> m_serviceHandlers;
  };

}
