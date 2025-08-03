#pragma once

//#include "acculine.h"
#include "inputprocessor.h"
#include "msg.h"

#include "enum.h"

//#include <utility>
#include <boost/optional.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/asio.hpp>

#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <memory>
#include <array>
#include <vector>

namespace uzel
{
  BETTER_ENUM(Priority, int8_t, undefined=-127, low = 0, high = 10); //NOLINT
  BETTER_ENUM(Direction, int8_t, incoming = 0, outgoing); //NOLINT

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


  explicit session(asiotcp::socket socket, Direction direction, boost::asio::ip::address ip, std::string remoteHostName = "");
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
  boost::signals2::signal<void (uzel::Msg::shr_t msg)> s_dispatch;
  boost::signals2::signal<void (const std::string &hostname)> s_connect_error;
  boost::signals2::signal<void ()> s_send_error;
  boost::signals2::signal<void ()> s_recv_error;
  boost::signals2::signal<void ()> s_connected;
  boost::signals2::signal<void (session::shr_t ss)> s_closed;
    // NOLINTEND(misc-non-private-member-variables-in-classes,cppcoreguidelines-non-private-member-variables-in-classes)

  Priority priority() const { return m_priority;}
  void setPriority(Priority p) { m_priority = p;}

  [[nodiscard]] bool outQueueEmpty() const;

  const uzel::Msg& msg1() const {return *m_msg1;}
  void start();
  void putOutQueue(uzel::Msg::shr_t msg);
    // take over processing of the messages from another (old) session
  void takeOverMessages(session &os);
  void takeOverMessages(MsgQueue &oq);
  void setRemoteIp(const boost::asio::ip::address &ip) {m_remoteIp = ip;}
  const boost::asio::ip::address& remoteIp() const{ return m_remoteIp;}
  void setRemoteHostName(const std::string &hname) {m_remoteHostName = hname;}
  const std::string& remoteHostName() const{ return m_remoteHostName;}
  void setRemoteNode(const std::string &node) { m_remoteNode = node;}
  const std::string& remoteNode() const{ return m_remoteNode;}
  [[nodiscard]] bool authenticated() const { return !m_remoteNode.empty();}


  [[nodiscard]] Direction direction() const {return m_direction;}

private:
  void deleteOld();
  void do_read();
  void do_write();
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
  uzel::Msg::shr_t m_msg1; // the very first message (used for auth)
  bool m_closeFlag{false};
  bool m_stopped{false};
  std::string m_reason{};
  std::string m_remoteHostName{}; //!< is set only if Direction::outgoing
  std::string m_remoteNode{}; //!< is set only after authorization
  boost::asio::ip::address m_remoteIp{};
  boost::signals2::connection m_rejected_c;
  boost::signals2::connection m_authorized_c;
  boost::signals2::connection m_dispatch_c;
//  boost::signals2::connection m_send_error_c;
//  boost::signals2::connection m_recv_error_c;
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
