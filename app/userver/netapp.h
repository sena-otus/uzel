#pragma once

#include <uzel/uzel.h>
#include <uzel/aresolver.h>
#include "session.h"
#include <utility>
#include <boost/asio.hpp>
#include <list>
#include <map>
#include <memory>

class session;

/** @brief приложение TCP сервер */
class NetServer
{
public:
    /**
     * @brief конструктор
     * @param io_context контекст ввода-вывода бустасио
     * @param port номер TCP порта на котором сервер будет принимать соединения */
  NetServer(boost::asio::io_context& io_context, unsigned short port);


  void auth(session::shr_t ss);
  void dispatch(uzel::Msg &msg);
private:
  void do_accept();
  void serviceMsg(uzel::Msg &msg);
  void localMsg(uzel::Msg &msg);
  void remoteMsg(uzel::Msg &msg);
  void broadcastMsg(uzel::Msg &msg);
  void localbroadcastMsg(uzel::Msg &msg);
  void connectResolved( boost::system::error_code ec,  boost::asio::ip::tcp::resolver::results_type rezit);

  boost::asio::ip::tcp::acceptor m_acceptor;
  std::map<std::string, session::shr_t> m_locals;
  std::map<std::string, session::shr_t> m_remotes;
  std::map<std::string, std::list<session::shr_t>> m_remotes2; // secondary connections

  aresolver m_aresolver;
  boost::asio::io_context& m_iocontext;
};
