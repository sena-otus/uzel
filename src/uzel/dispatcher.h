#pragma once

#include "msg.h"

namespace uzel {

  class MsgDispatcher {
  public:
    void registerHandler(const std::string& cname, std::function<void(const Msg&)> handler) {
      m_handlers[cname] = std::move(handler);
    }

    void dispatch(const Msg& msg) const {
      auto it = m_handlers.find(msg.cname());
      if (it != m_handlers.end()) {
        it->second(msg);
      }
    }

  private:
    std::unordered_map<std::string, std::function<void(const Msg&)>> m_handlers;
  };

  template<class SessionT>
  class SessionMsgDispatcher {
  public:
    void registerHandler(const std::string& cname, std::function<void(const Msg&, typename SessionT::shr_t ss)> handler) {
      m_serviceHandlers[cname] = std::move(handler);
    }

    void dispatch(const Msg& msg, SessionT::shr_t ss) const {
      auto it = m_serviceHandlers.find(msg.cname());
      if (it != m_serviceHandlers.end()) {
        it->second(msg,ss);
      }
    }

  private:
    std::unordered_map<std::string, std::function<void(const Msg&, typename SessionT::shr_t ss)>> m_serviceHandlers;
  };

}
