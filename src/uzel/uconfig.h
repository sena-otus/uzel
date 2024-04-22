#pragma once

#include <boost/property_tree/ptree.hpp>
#include <string>
#include <list>

namespace uzel
{


  class UConfig
  {
  public:
    using ptree = boost::property_tree::ptree;

    UConfig();
    [[nodiscard]] std::string nodeName() const;
    [[nodiscard]] static std::string appName();
    [[nodiscard]] bool isLocalNode(const std::string &nname) const;
    std::list<std::string> remotes() const;
  private:
    ptree m_pt;
  };


  class UConfigS
  {

  public:
    static UConfig& getUConfig();
  };

};
