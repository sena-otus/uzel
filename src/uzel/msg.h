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
    enum class DestType
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
    Msg(const Addr &dest, Msg::ptree && body);
      /** ctor forwarding message
       * used to forward incoming message
       * @param to where to forward
       * @param other original message
       **/
    Msg(const Addr &dest, Msg &&other);
    void addPayload();
    [[nodiscard]] DestType destType() const;
    [[nodiscard]] bool isBroadcast() const;
    [[nodiscard]] bool isLocal() const;
    [[nodiscard]] bool fromLocal() const;
    [[nodiscard]] bool isRemote() const;
    [[nodiscard]] bool isLocalBroadcast() const;
    [[nodiscard]] std::string str() const;
    [[nodiscard]] std::string move_tostr();
    [[nodiscard]] std::vector<char> moveToCharvec();
    [[nodiscard]] std::vector<char> charvec() const;
    [[nodiscard]] Addr from() const;
    [[nodiscard]] Addr dest() const;
    [[nodiscard]] const ptree& header() const;
      /** that will parse the body if needed, may throw */
    [[nodiscard]] const ptree& pbody() const;
  private:
    void setDest(const Addr &dest);
    void updateDest();
    void updateFrom();
    void setFromLocal();

    ptree m_header; //!< message header
    mutable std::variant<std::vector<char>,boost::property_tree::ptree> m_body; //!< unpared/parsed message body
    DestType m_destType;
      // cached values:
    Addr m_dest;
    Addr m_from;
  };


};
