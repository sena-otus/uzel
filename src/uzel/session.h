#pragma once

//#include "acculine.h"
#include "inputprocessor.h"
#include "msg.h"

#include "enum.h"

//#include <utility>
#include <boost/optional.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/asio.hpp>
#include <memory>
#include <array>
#include <queue>

namespace uzel
{
  BETTER_ENUM(Priority, int8_t, undefined=-127, low = 0, high = 10); //NOLINT
  BETTER_ENUM(Direction, int8_t, incoming = 0, outgoing); //NOLINT



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


  explicit session(asiotcp::socket socket, Direction direction);
  ~session();

  session(const session &other) = delete;
  session(session &&other) = delete;
  session& operator=(session &&other) = delete;
  session& operator=(const session &other) = delete;

  void disconnect();
  void startConnection(const asiotcp::resolver::results_type &remote);
  void connectHandler(const boost::system::error_code &ec, const asiotcp::resolver::results_type &remote);

    // NOLINTBEGIN(misc-non-private-member-variables-in-classes,cppcoreguidelines-non-private-member-variables-in-classes)
  boost::signals2::signal<void (session::shr_t ss)> s_auth;
  boost::signals2::signal<void (uzel::Msg &msg)> s_dispatch;
  boost::signals2::signal<void (const std::string &hostname)> s_connect_error;
  boost::signals2::signal<void ()> s_send_error;
  boost::signals2::signal<void ()> s_receive_error;
    // NOLINTEND(misc-non-private-member-variables-in-classes,cppcoreguidelines-non-private-member-variables-in-classes)

  Priority priority() const { return m_priority;}
  void setPriority(Priority p) { m_priority = p;}


  const uzel::Msg& msg1() const {return m_msg1;}
  void start();
  void putOutQueue(const uzel::Msg& msg);
  void putOutQueue(uzel::Msg&& msg);
    // take over processing of the messages from another (old) session
  void takeOverMessages(session &os);
  void takeOverMessages(std::queue<std::string> &oq);
private:
  void do_read();
  void do_write();

  boost::asio::ip::tcp::socket m_socket;
  Priority m_priority{Priority::undefined};
  Direction m_direction;
  enum { max_length = 1024*1024 };
  std::array<char, max_length> m_data;
  uzel::InputProcessor m_processor;
  std::queue<std::string> m_outQueue;
  uzel::Msg m_msg1; // the very first message (used for auth)
  boost::signals2::connection m_rejected_c;
  boost::signals2::connection m_authorized_c;
  boost::signals2::connection m_dispatch_c;
};


//   using BPriorityTranslator = BEnumTranslator<uzel::Priority>;

// // Register translators
//   namespace boost { namespace property_tree {
//       template<typename Ch, typename Traits, typename Alloc>
//       struct translator_between<std::basic_string<Ch, Traits, Alloc>, uzel::Priority> {
//         using type = BPriorityTranslator;
//       };
//     }
//   }



}
