#pragma once

#include "msg.h"
#include "session.h"

namespace uzel {

  class Dispatcher {
  public:
    void registerHandler(const std::string& cname, std::function<void(const Msg&)> handler) {
      m_handlers[cname] = std::move(handler);
    }
    void registerHandler(const std::string& cname, std::function<void(const Msg&, session::shr_t ss)> handler) {
      m_serviceHandlers[cname] = std::move(handler);
    }

    void dispatch(const Msg& msg) const {
      auto it = m_handlers.find(msg.cname());
      if (it != m_handlers.end()) {
        it->second(msg);
      }
    }

    void dispatch(const Msg& msg, session::shr_t ss) const {
      auto it = m_serviceHandlers.find(msg.cname());
      if (it != m_serviceHandlers.end()) {
        it->second(msg,ss);
      }
    }

  private:
    std::unordered_map<std::string, std::function<void(const Msg&)>> m_handlers;
    std::unordered_map<std::string, std::function<void(const Msg&, session::shr_t ss)>> m_serviceHandlers;
  };

}
