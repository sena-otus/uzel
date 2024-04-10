#pragma once

#include "acculine.h"
#include <boost/asio/ip/address_v4.hpp>
#include <boost/property_tree/ptree.hpp>
#include <variant>
#include <string>
#include <queue>

class GConfig
{
public:
  static GConfig& GConfigS();
  std::string nodeName();
private:
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
      local, remote, broadcast
    };
    using ptree = boost::property_tree::ptree;
    explicit Msg(ptree &&header, std::string &&body);
    explicit Msg(ptree &&header, ptree &&body);
    void addPayload();
    DestType destType() const;
    bool isBroadcast() const;
    bool isLocal() const;
    bool isRemote() const;

  private:
    void checkDestHost();

    boost::property_tree::ptree m_header; //!< message header
    std::variant<std::string,boost::property_tree::ptree> m_body; //!< unpared/parsed message body
    DestType m_destType;
  };

  class MsgQueue
  {
  public:
    bool processNewInput(std::string_view input);
    void processQueue();
  private:
    std::queue<Msg> m_mqueue;
    AccuLine m_acculine;
    std::optional<boost::property_tree::ptree> m_header;
  };

};
