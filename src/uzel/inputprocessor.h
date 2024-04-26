#pragma once

#include "acculine.h"
#include "addr.h"

#include <boost/property_tree/ptree.hpp>
#include <boost/signals2.hpp>
#include <string_view>
#include <optional>

namespace uzel
{

  class Msg;

  class InputProcessor
  {
  public:
    bool processNewInput(std::string_view input);
    void processMsg(Msg && msg);
    [[nodiscard]] bool auth() const;
    bool  auth(const Msg &msg);

    boost::signals2::signal<void ()> s_rejected{};
    boost::signals2::signal<void (const Msg &msg1)> s_auth{};
    boost::signals2::signal<void (Msg &msg)> s_dispatch{};
  private:
    AccuLine m_acculine{};
    std::optional<boost::property_tree::ptree> m_header{};
    Addr m_peer{};
  };

};
