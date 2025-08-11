#pragma once

//#include "acculine.h"
#include "inputprocessor.h"
#include "msg.h"
#include "dispatcher.h"

#include "enum.h"

//#include <utility>
#include <boost/optional.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/asio.hpp>

#include <charconv>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <memory>
#include <array>
#include <vector>

namespace uzel
{
    // uint8_t does not work well with boost::ptree...
  BETTER_ENUM(Priority, uint16_t, undefined=255, low = 0, high = 10); //NOLINT
  BETTER_ENUM(Direction, uint8_t, incoming = 0, outgoing); //NOLINT

  inline const int64_t MessageTTL_sec = 60;

  struct QueuedMsg
  {
    explicit QueuedMsg(Msg::shr_t msg)
      : m_rawmsg(msg->charvec()), m_dt(msg->destType()), m_enqueueTime(std::chrono::steady_clock::now())
    {
    }

    [[nodiscard]] Msg::DestType destType() const { return m_dt; }
    [[nodiscard]] std::chrono::steady_clock::time_point enqueueTime() const { return m_enqueueTime;}
    [[nodiscard]] const std::vector<char> &rawmsg() const { return m_rawmsg;}


  private:
    std::vector<char> m_rawmsg;
    Msg::DestType m_dt;
    std::chrono::steady_clock::time_point m_enqueueTime;
  };

  using MsgQueue = std::deque<QueuedMsg>;

  class NetAppContext;
  using NetAppContextPtr = std::shared_ptr<NetAppContext>;
/**
 * class session handles one single (tcp) connection (there can be several between two remotes)
 * For incoming messages it uses uzel::Inputprocessor for initial
 * message parsing and dispatch, outgoing messages are queued in m_outQueue
 *  */
class session
  : public std::enable_shared_from_this<session>
{
public:
  using shr_t = std::shared_ptr<session>;
  using asiotcp = boost::asio::ip::tcp;

  explicit session(NetAppContextPtr netctx, asiotcp::socket socket, Direction direction, boost::asio::ip::address ip, std::string remoteHostName = "");
  ~session();

  session(const session &other) = delete;
  session(session &&other) = delete;
  session& operator=(session &&other) = delete;
  session& operator=(const session &other) = delete;

    /** notify peer and shut down */
  void gracefullClose(const std::string &reason);
  void startConnection(const asiotcp::resolver::results_type &remote);
  void connectHandler(const boost::system::error_code &ec, const asiotcp::resolver::results_type &remote);

    // NOLINTBEGIN(misc-non-private-member-variables-in-classes,cppcoreguidelines-non-private-member-variables-in-classes)
  boost::signals2::signal<void (session::shr_t ss)> s_auth;
  boost::signals2::signal<void (uzel::Msg::shr_t msg)> s_forward;
  boost::signals2::signal<void (const std::string &hostname)> s_connect_error;
  boost::signals2::signal<void ()> s_send_error;
  boost::signals2::signal<void ()> s_recv_error;
  boost::signals2::signal<void ()> s_connected;
  boost::signals2::signal<void (session::shr_t ss)> s_closed;
    // NOLINTEND(misc-non-private-member-variables-in-classes,cppcoreguidelines-non-private-member-variables-in-classes)

  Priority priority() const { return m_priority;}
  void setPriority(Priority p) { m_priority = p;}

  [[nodiscard]] bool outQueueEmpty() const;

  void start();
  void putOutQueue(uzel::Msg::shr_t msg);
    // take over processing of the messages from another (old) session
  void takeOverMessages(session &os);
  void takeOverMessages(MsgQueue &oq);
  void setRemoteIp(const boost::asio::ip::address &ip) {m_remoteIp = ip;}
  const boost::asio::ip::address& remoteIp() const{ return m_remoteIp;}
  void setRemoteHostName(const std::string &hname) {m_remoteHostName = hname;}
  const std::string& remoteHostName() const{ return m_remoteHostName;}
  const std::string& peerNode() const;
  const std::string& peerApp() const;
  [[nodiscard]] bool authenticated() const { return m_msg1 != nullptr;}
  [[nodiscard]] Direction direction() const {return m_direction;}
  void dispatchMsg(Msg::shr_t msg);
  bool peerIsLocal() const;
private:
  void deleteOld();
  void do_read();
  void do_write();
  bool authenticate(uzel::Msg::shr_t msg);
  void sendByeAndClose();
    /**
     *  shutdown socket and release all related resources, emits signal
     *  s_closed(shared_from_this()), so it can not be called from ~session()
     *  */
  void stop();
    /**
     * Same as a stop() above but can be called from destructor as it
     * does not call emits signal s_closed(shared_from_this())
     * */
  void shutdown();

  boost::asio::ip::tcp::socket m_socket;
  Priority m_priority{Priority::undefined};
  Direction m_direction;
  enum { max_length = 1024*1024 };
  std::array<char, max_length> m_data;
  uzel::InputProcessor m_processor;
  MsgQueue m_outQueue;
  uzel::Msg::shr_t m_msg1; // the very first message is set only after authentication
  bool m_closeFlag{false};
  bool m_stopped{false};
  std::string m_reason{};
  std::string m_remoteHostName{}; //!< is set only if Direction::outgoing
  boost::asio::ip::address m_remoteIp{};
  NetAppContextPtr m_netctx;
};

  struct AddressHash {
    std::size_t operator()(const boost::asio::ip::address& addr) const {
      return std::hash<std::string>()(addr.to_string());
    }
  };


  struct AddressEqual {
    bool operator()(const boost::asio::ip::address& lhs, const boost::asio::ip::address& rhs) const {
      return lhs == rhs;
    }
  };

  using IpToSession = std::unordered_map<boost::asio::ip::address,
                     std::unordered_set<uzel::session::shr_t>,
                     AddressHash, AddressEqual>;

}

inline std::ostream& operator<<(std::ostream& os, const uzel::Priority pr)
{
  return os << static_cast<int>(pr._to_integral());
}

inline std::istream& operator>>(std::istream& is, uzel::Priority& pr)
{
  int iPriority{0};
  if (!(is >> iPriority)) return is;
  auto rez = uzel::Priority::_from_integral_nothrow(iPriority);
  if(!rez) {
    is.setstate(std::ios::failbit); // invalid token
  } else {
    pr = *rez;
  }
  return is;
}

/** translator for property tree */

template<typename EnumClass>
struct BEnumIntTranslator {
  using internal_type = std::string;
  using external_type = EnumClass;

  boost::optional<external_type> get_value(const internal_type& str) {
    int val{};
    auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), val);
    if (ec != std::errc()) {
      return boost::optional<external_type>(boost::none);
    }
    auto maybe_val = EnumClass::_from_integral_nothrow(val);
    if(maybe_val) {
      return boost::optional<external_type>(*maybe_val);
    }
    return boost::optional<external_type>(boost::none);
  }

  boost::optional<internal_type> put_value(const external_type& value) {
    return boost::optional<internal_type>(std::to_string(value._to_integral()));
  }
};


using PriorityTranslator = BEnumIntTranslator<uzel::Priority>;

// Register translators
namespace boost::property_tree {
  template<typename Ch, typename Traits, typename Alloc>
  struct translator_between<std::basic_string<Ch, Traits, Alloc>, uzel::Priority>
  {
    using type = PriorityTranslator;
  };
}

namespace boost::property_tree {
  template<>
  struct translator_between<std::string, uzel::Priority>
  {
    using type = PriorityTranslator;
  };

} // namespace boost::property_tree
