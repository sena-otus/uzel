#pragma once

#include <utility>
#include <boost/asio.hpp>
#include <list>
#include <map>

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

private:
  void do_accept();

  boost::asio::ip::tcp::acceptor m_acceptor;
  std::list<std::shared_ptr<session>> m_unauthlist;
  std::map<std::string, std::shared_ptr<session>> m_locals;
  std::map<std::string, std::shared_ptr<session>> m_remotes;
};
