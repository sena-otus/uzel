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

    /**
     * InputProcessor performes initial parsing of incoming messages
     * and dispatch them to consumers which must be connected to the
     * s_dispatch() signal.
     *
     * Currently only json messages are supported. Each message has
     * message header and message body. No new lines inside
     * the message body or message header is allowed.
     *
     * Big messages can be splitted into 2 parts, separated by new
     * line, the message header and the message body. Small messages can
     * be sent in one line. Splitted messages allows to parse header
     * only, the unparsed part can be forwarded as a string to
     * the receiver.
     * */
  class InputProcessor
  {
  public:
      /** process new input string */
    bool processNewInput(std::string_view input);
    void processMsg(Msg && msg);
    [[nodiscard]] bool auth() const;
    bool  auth(const Msg &msg);

      /** signal fired if authentication failed */
    boost::signals2::signal<void ()> s_rejected{};
      /** signal fired on successful authentication */
    boost::signals2::signal<void (const Msg &msg1)> s_auth{};
      /** signal fired if there is a message to dispatch
       * @param msg message to process */
    boost::signals2::signal<void (Msg &msg)> s_dispatch{};
  private:
      /** process input stream, parse messages */
    AccuLine m_acculine{};
      /** temporal storage for the message header */
    std::optional<boost::property_tree::ptree> m_header;
    Addr m_peer;
  };

};
