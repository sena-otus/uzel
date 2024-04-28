#pragma once

#include <uzel/aresolver.h>
#include <uzel/session.h>
#include <utility>
#include <boost/asio.hpp>
#include <list>
#include <map>
#include <memory>

class session;

/** @brief приложение TCP сервер */
class NetClient: public std::enable_shared_from_this<NetClient>
{
public:
    /**
     * @brief конструктор
     * @param io_context контекст ввода-вывода бустасио
     * @param port номер TCP порта к которому будем коннектиться */
  NetClient(boost::asio::io_context& io_context, unsigned short port);


  void auth(session::shr_t ss);
  void dispatch(uzel::Msg &msg);
  void start();
private:
  void do_accept();
  void serviceMsg(uzel::Msg &msg);
  void localMsg(uzel::Msg &msg);
  void remoteMsg(uzel::Msg &msg);
  void broadcastMsg(uzel::Msg &msg);
  void localbroadcastMsg(uzel::Msg &msg);
  void connectResolved( boost::system::error_code ec,  boost::asio::ip::tcp::resolver::results_type rezit);

  std::map<std::string, session::shr_t> m_locals;
  aresolver m_aresolver;
  boost::optional<boost::asio::io_context::work> m_work;
};
