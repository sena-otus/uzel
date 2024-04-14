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
    Addr(std::string appname, std::string hostname);
  private:
    std::string m_appname;
    std::string m_hostname;
  };

  class Msg
  {
  public:
    enum DestType
    {
      local, remote, broadcast, localbroadcast
    };
    using ptree = boost::property_tree::ptree;
    explicit Msg(ptree &&header, std::string &&body);
    explicit Msg(ptree &&header, ptree &&body);
    void addPayload();
    DestType destType() const;
    bool isBroadcast() const;
    bool isLocal() const;
    bool isRemote() const;
    bool isLocalBroadcast() const;
    std::string str() const;
    const ptree& header() const;
  private:
    void checkDest();

    ptree m_header; //!< message header
    std::variant<std::string,boost::property_tree::ptree> m_body; //!< unpared/parsed message body
    DestType m_destType;
  };

  class InputProcessor
  {
  public:
    bool processNewInput(std::string_view input);
    void processMsg(Msg && msg);
    bool isLocal() const;
    bool auth() const;
    bool  auth(const Msg::ptree &header);

    boost::signals2::signal<void ()> s_disconnect;
    boost::signals2::signal<void (Msg &msg)> s_broadcast;
    boost::signals2::signal<void (Msg &msg)> s_remote;
    boost::signals2::signal<void (Msg &msg)> s_local;
    boost::signals2::signal<void (Msg &msg)> s_localbroadcast;
  private:
    AccuLine m_acculine;
    std::optional<boost::property_tree::ptree> m_header;
    std::string m_remoteHost;
    std::string m_remoteName;
    bool m_isLocal{true};
  };

};
