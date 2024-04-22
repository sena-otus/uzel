#pragma once

#include "acculine.h"
#include "uconfig.h"
#include <boost/asio/ip/address_v4.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/signals2.hpp>
#include <variant>
#include <string>
#include <string_view>
#include <queue>



namespace uzel
{

  class Addr
  {
  public:
    Addr() = default;
    Addr(std::string appname, std::string nodename);
    [[nodiscard]] std::string app() const {return m_appname;}
    [[nodiscard]] std::string node() const {return m_nodename;}
  private:
    std::string m_appname;
    std::string m_nodename;
  };

  class Msg
  {
  public:
    enum DestType
    {
      local, remote, broadcast, localbroadcast
    };
    using ptree = boost::property_tree::ptree;
    using hdr_t = ptree;
      /*** construct incoming message */
    explicit Msg(ptree &&header, std::string &&body);
      /*** construct incoming message */
    explicit Msg(ptree &&header, ptree &&body);
    void addPayload();
    [[nodiscard]] DestType destType() const;
    [[nodiscard]] bool isBroadcast() const;
    [[nodiscard]] bool isLocal() const;
    [[nodiscard]] bool isRemote() const;
    [[nodiscard]] bool isLocalBroadcast() const;
    [[nodiscard]] std::string str() const;
    [[nodiscard]] std::string move_tostr();
    [[nodiscard]] const Addr& from() const;
    [[nodiscard]] const Addr& dest() const;
    [[nodiscard]] const ptree& header() const;
  private:
    void updateDest();
    void updateFrom();

    ptree m_header; //!< message header
    std::variant<std::string,boost::property_tree::ptree> m_body; //!< unpared/parsed message body
    DestType m_destType;
      // cached values:
    Addr m_dest;
    Addr m_from;
  };

  class InputProcessor
  {
  public:
    bool processNewInput(std::string_view input);
    void processMsg(Msg && msg);
    [[nodiscard]] bool isLocal() const;
    [[nodiscard]] bool auth() const;
    bool  auth(const Msg &msg);

    boost::signals2::signal<void ()> s_rejected;
    boost::signals2::signal<void (const Msg &msg1)> s_auth;
    boost::signals2::signal<void (Msg &msg)> s_dispatch;
  private:
    AccuLine m_acculine;
    std::optional<boost::property_tree::ptree> m_header;
    Addr m_peer;
    bool m_isLocal{true};
  };

};
