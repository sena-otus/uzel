#pragma once

#include "acculine.h"
#include <boost/asio/ip/address_v4.hpp>
#include <boost/property_tree/ptree.hpp>
#include <variant>
#include <string>
#include <queue>

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
    using ptree = boost::property_tree::ptree;
    explicit Msg(ptree &&header, std::string &&body);
    explicit Msg(ptree &&header, ptree &&body);
    void addPayload();
  private:
    boost::property_tree::ptree m_header; //!< message header
    std::variant<std::string,boost::property_tree::ptree> m_body; //!< unpared/parsed message body
  };

  class MsgQueue
  {
  public:
    bool processNewInput(std::string_view input);
  private:
    std::queue<Msg> m_mqueue;
    AccuLine m_acculine;
    std::optional<boost::property_tree::ptree> m_header;
  };

};
