#pragma once

#include "acculine.h"
#include "uconfig.h"
#include "addr.h"

#include <boost/property_tree/ptree.hpp>
//#include <boost/asio/ip/address_v6.hpp>
#include <variant>
#include <string>
#include <string_view>
#include <queue>



namespace uzel
{
  class Msg
  {
  public:
    enum DestType
    {
      local, remote, broadcast, localbroadcast, service
    };
    using ptree = boost::property_tree::ptree;
    using hdr_t = ptree;
      /*** construct incoming message */
    explicit Msg(ptree &&header, std::string &&body);
      /*** construct incoming message */
    explicit Msg(ptree &&header, ptree &&body);
      /** construct outgoing message */
    Msg(const std::string &appname, const std::string &hname, Msg::ptree && body);
    void addPayload();
    [[nodiscard]] DestType destType() const;
    [[nodiscard]] bool isBroadcast() const;
    [[nodiscard]] bool isLocal() const;
    [[nodiscard]] bool fromLocal() const;
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


};
