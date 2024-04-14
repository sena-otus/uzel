#pragma once

#include "acculine.h"
#include <boost/asio/ip/address_v4.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/signals2.hpp>
#include <variant>
#include <string>
#include <string_view>
#include <queue>

class GConfig
{
public:
  std::string nodeName() const;
  bool isLocalNode(const std::string &nname) const;
private:
};


class GConfigS
{

public:
  static GConfig& getGConfig();
};





namespace uzel
{

  class Addr
  {
  public:
    Addr() = default;
    Addr(std::string appname, std::string nodename);
    std::string app() const {return m_appname;}
    std::string node() const {return m_nodename;}
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
    DestType destType() const;
    bool isBroadcast() const;
    bool isLocal() const;
    bool isRemote() const;
    bool isLocalBroadcast() const;
    std::string str() const;
    const Addr& from() const;
    const Addr& dest() const;
    const ptree& header() const;
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
    bool isLocal() const;
    bool auth() const;
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
