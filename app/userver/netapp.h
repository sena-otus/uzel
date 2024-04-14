#pragma once

#include <uzel/uzel.h>
#include "session.h"
#include <utility>
#include <boost/asio.hpp>
#include <list>
#include <map>
#include <memory>

class session;

/** @brief приложение TCP сервер */
class NetApp
{
public:
    /**
     * @brief конструктор
     * @param io_context контекст ввода-вывода бустасио
     * @param port номер TCP порта на котором сервер будет принимать соединения */
  NetApp(boost::asio::io_context& io_context, unsigned short port);


  void auth(session::shr_t ss);
  void dispatch(uzel::Msg &msg);
private:
  void do_accept();
  void localMsg(uzel::Msg &msg);
  void remoteMsg(uzel::Msg &msg);
  void broadcastMsg(uzel::Msg &msg);
  void localbroadcastMsg(uzel::Msg &msg);

  boost::asio::ip::tcp::acceptor m_acceptor;
  std::map<std::string, session::shr_t> m_locals;
  std::map<std::string, session::shr_t> m_remotes;
};
