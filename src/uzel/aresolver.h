#pragma once

#define BOOST_BIND_GLOBAL_PLACEHOLDERS

#include <utility> //NOLINT{unused-includes}
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/optional.hpp>
#include <boost/thread.hpp>

namespace uzel
{

/// @brief Asynchronous host resolution with a dedicated thread pool.
  class AResolver
  {
  public:
    AResolver(const std::size_t pool_size, boost::asio::io_context &mainiocontext)
      : m_mainiocontext(mainiocontext), m_work(boost::ref(m_iocontext)) {
        // Create pool.
      for (std::size_t i = 0; i < pool_size; ++i)
        m_threads.create_thread([this]{m_iocontext.run();});
    }

    ~AResolver()
      {
        m_work = boost::none;
        m_threads.join_all();
      }

    AResolver(const AResolver &) = delete;
    AResolver(AResolver &&) = delete;
    AResolver &operator=(const AResolver &) = delete;
    AResolver &operator=(AResolver &&) = delete;


    template <class Protocol>
    using resolve_handler_t = std::function<void(boost::system::error_code, typename Protocol::resolver::results_type)>;

    template <class Protocol>
    void async_resolve(std::string host, std::string service,  resolve_handler_t<Protocol> handler)
      {
        boost::asio::post(m_iocontext,
                          boost::bind(&AResolver::do_async_resolve<Protocol>, this, host, service, handler)
                          );
      }

  private:
      /// @brief Resolve address and invoke continuation handler.
    template <class Protocol>
    void do_async_resolve(std::string host, std::string service, resolve_handler_t<Protocol> handler)
      {
          // Resolve synchronously, as synchronous resolution will perform work
          // in the calling thread.  Thus, it will not use Boost.Asio's internal
          // thread that is used for asynchronous resolution.
        boost::system::error_code error;
        typename Protocol::resolver resolver(m_iocontext);

        auto rezult = resolver.resolve(host, service, error);
          // Invoke user handler
        boost::asio::post(boost::asio::make_strand(m_mainiocontext),  boost::bind(handler, error, rezult));
      }


    boost::asio::io_context m_iocontext;
    boost::asio::io_context &m_mainiocontext;
    boost::optional<boost::asio::io_context::work> m_work;
    boost::thread_group m_threads;
  };

}
