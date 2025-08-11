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
  class session;
  using SessionWPtr = std::weak_ptr<session>;

  class Msg
  {
  public:
    using shr_t = std::shared_ptr<Msg>;
    using shr_const_t = std::shared_ptr<const Msg>;

    enum class DestType
    {
      service, local, remote, broadcast, localbroadcast
    };
    using ptree = boost::property_tree::ptree;
    using hdr_t = ptree;
      /*** construct incoming message */
    explicit Msg(ptree &&header, std::string &&body, SessionWPtr sswptr);
      /*** construct incoming message */
    explicit Msg(ptree &&header, ptree &&body, SessionWPtr sswptr);
      /** construct outgoing message */
    Msg(const Addr &dest, const std::string &cname, Msg::ptree && body);
      /** ctor forwarding message
       * used to forward incoming message
       * @param to where to forward
       * @param other original message
       **/
    Msg(const Addr &dest, Msg &&other);
    Msg(const Addr &dest, const Msg &other);
      // void addPayload();
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
    [[nodiscard]] const Addr& from() const;
    [[nodiscard]] const Addr& dest() const;
    [[nodiscard]] const ptree& header() const;
      /** that will parse the body if needed, may throw */
    [[nodiscard]] const ptree& pbody() const;
      /** throws if there is no cname in header */
    [[nodiscard]] const std::string &cname() const;
    [[nodiscard]] bool toMe() const;

      /** remember from which session msg came (only for incoming) */
    void setOrigin(std::weak_ptr<session> s) { m_origin = std::move(s); }
      /** get originating session */
    std::weak_ptr<session> origin() const { return m_origin; }
  private:
    void setDest(const Addr &dest);
    void setCname(const std::string &cname);
    void updateDest();
    void updateFrom();
    void setFromLocal();

    ptree m_header; //!< message header
    mutable std::variant<std::vector<char>,boost::property_tree::ptree> m_body; //!< unparsed/parsed message body
      // cached values (not serialized):
    DestType m_destType;
    Addr m_dest;
    Addr m_from;
    bool m_toMe{false}; // set by updateDest()
    SessionWPtr m_origin;
  };


};
