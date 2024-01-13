// lib.cpp : Defines the entry point for the application.
//

#include <asiopq/Connection.hpp>
#include <asiopq/Exception.hpp>
#include <asiopq/NotifyPtr.hpp>
#include <asiopq/PQMemory.hpp>
#include <asiopq/ResultPtr.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/cobalt/async_for.hpp>
#include <boost/cobalt/generator.hpp>
#include <boost/cobalt/op.hpp>
#include <boost/cobalt/promise.hpp>
#include <boost/cobalt/this_coro.hpp>
#include <boost/cobalt/this_thread.hpp>
#include <libpq-fe.h>
#include <memory>
#include <stdexcept>
#include <utility>

namespace PC::asiopq
{
   namespace
   {
      inline auto create_socket(::boost::asio::any_io_executor executor,
                                int const                      socket_fd)
      {
         using Socket = Connection::SocketPtr::element_type;
         return ::std::make_unique<Socket>(
             ::std::move(executor), Socket::protocol_type{}, socket_fd);
      }
   } // namespace
   Connection::~Connection()
   {
      if (conn != nullptr)
         PQfinish(conn);
   }

   boost::cobalt::generator<NotifyPtr>
       Connection::await_notify_async(::std::string_view command)
   {
      {
         auto const res = co_await command_async(command);
         if (not res)
         {
            throw asiopq::Exception{*this, res};
         }
      }
      auto op = await_notify_async();
      BOOST_COBALT_FOR(auto val, op)
      {
         co_yield ::std::move(val);
      }
      co_return co_await op;
   }
   boost::cobalt::generator<NotifyPtr> Connection::await_notify_async()
   {
      while (true)
      {
         co_await wait_for_read_async();
         if (PQconsumeInput(conn) != 1)
         {
            throw asiopq::Exception{*this};
         }
         while (true)
         {
            auto result{PQnotifies(conn)};
            if (result == nullptr)
               break;
            if (PQconsumeInput(conn) != 1)
            {
               throw asiopq::Exception{*this};
            }
            co_yield result;
         }
      }
      co_return NotifyPtr{};
   }

   boost::cobalt::generator<ResultPtr> Connection::wait_for_response()
   {
      while (true)
      {
         co_await wait_for_read_async();
         if (PQconsumeInput(conn) != 1)
         {
            throw asiopq::Exception{*this};
         }
         while (::PQisBusy(conn) == 0)
         {
            auto* const ptr = PQgetResult(conn);
            if (ptr == nullptr)
            {
               co_return ResultPtr{nullptr};
            }
            ResultPtr result{ptr};
            co_yield ::std::move(result);
         }
      }
      co_return ResultPtr{nullptr};
   }

   boost::cobalt::generator<ResultPtr>
       Connection::commands_async(std::string_view command)
   {
      // Returns 1 on success
      if (PQsendQuery(conn, std::data(command)) != 1)
      {
         throw asiopq::Exception{*this};
      }
      auto op = wait_for_response();
      BOOST_COBALT_FOR(auto res, op)
      {
         co_yield ::std::move(res);
      }
      co_return co_await op;
   }

   boost::cobalt::generator<ResultPtr> Connection::stream_async(std::string_view command)
   {
      // Returns 1 on success
      if (PQsendQuery(conn, std::data(command)) != 1)
      {
         throw asiopq::Exception{*this};
      }
      // Returns 1 on success
      if (::PQsetSingleRowMode(conn) != 1)
      {
         throw asiopq::Exception{*this};
      }
      auto op = wait_for_response();
      BOOST_COBALT_FOR(auto res, op)
      {
         co_yield ::std::move(res);
      }
      co_return co_await op;
   }

   boost::cobalt::promise<ResultPtr> Connection::command_async(std::string_view command)
   {
      ResultPtr res;
      auto      op = commands_async(command);
      while (auto val = co_await op)
      {
         res = ::std::move(val);
      }
      co_return ::std::move(res);
   }
   void Connection::connect(std::string_view connection_string)
   {
      conn = PQconnectdb(std::data(connection_string));
   }

   PGconn* Connection::native_handle()
   {
      return conn;
   }

   boost::cobalt::promise<void> Connection::wait_for_read_async()
   {
      // Null buffers ensures we only wait
      // But do not end up reading anything
      co_await socket->async_read_some(boost::asio::null_buffers{},
                                       boost::cobalt::use_op);
   }

   boost::cobalt::promise<void>
       Connection::connect_async(std::string_view connection_string)
   {
      conn = PQconnectStart(std::data(connection_string));
      if (status() == ConnStatusType::CONNECTION_BAD)
         throw asiopq::Exception{*this};
      while (true)
      {
         // Make a poll to PQ
         auto const poll_res = PQconnectPoll(conn);
         switch (poll_res)
         {
            case PostgresPollingStatusType::PGRES_POLLING_OK:
            {
               if (::PQsetnonblocking(conn, 1) != 0)
               {
                  throw asiopq::Exception{*this};
               }
               co_return;
            }
            case PostgresPollingStatusType::PGRES_POLLING_FAILED:
            {
               throw asiopq::Exception{*this};
            }
            // If Read, wait for PQ to read from Socket
            case PostgresPollingStatusType::PGRES_POLLING_READING:
            {
               socket = create_socket(co_await boost::cobalt::this_coro::executor,
                                      dup_native_socket_handle());
               // Null buffers ensures we only wait
               // But do not end up reading anything
               co_await wait_for_read_async();
            }
            break;
            // If Write, wait for PQ to write to socket
            case PostgresPollingStatusType::PGRES_POLLING_WRITING:
            {
               socket = create_socket(co_await boost::cobalt::this_coro::executor,
                                      dup_native_socket_handle());
               // Null buffers ensures we only wait
               // But do not end up reading anything
               co_await socket->async_write_some(boost::asio::null_buffers{},
                                                 boost::cobalt::use_op);
            }
            break;
         }
      }
   }

   boost::cobalt::promise<ResultPtr>
       Connection::exec_cmmand_without_completion_wait(::std::string_view command)
   {
      // Returns 1 on success
      if (PQsendQuery(conn, std::data(command)) != 1)
      {
         throw asiopq::Exception{*this};
      }
      while (true)
      {
         co_await wait_for_read_async();
         if (PQconsumeInput(conn) != 1)
         {
            throw asiopq::Exception{*this};
         }
         while (::PQisBusy(conn) == 0)
         {
            auto* const ptr = PQgetResult(conn);
            ResultPtr   result{ptr};
            co_return ::std::move(result);
         }
      }
      co_return ResultPtr{nullptr};
   }

   boost::cobalt::generator<PQMemory<char> /*Buffer*/>
       Connection::copy_from_db(::std::string_view command)
   {
      /// @note First execute the query
      {
         auto result = co_await exec_cmmand_without_completion_wait(command);
         if (result.status() != PGRES_COPY_OUT)
         {
            throw asiopq::Exception{*this};
         }
      }
      using Result = PQMemory<char>;
      while (true)
      {
         Result     result;
         auto const sz = PQgetCopyData(native_handle(), result.ref_ptr(), true /*Async*/);
         /// @note A result of -1 indicates the copy is over
         /// @note https://www.postgresql.org/docs/current/libpq-copy.html
         if (sz == -1)
         {
            break;
         }
         /// @note A result of -2 indicates an error
         /// @note https://www.postgresql.org/docs/current/libpq-copy.html
         if (sz == -2)
         {
            throw asiopq::Exception{*this};
         }
         /// @note Returns 0 in async if no data is available for immediate return
         if (sz == 0)
         {
            co_await wait_for_read_async();
            if (PQconsumeInput(conn) != 1)
            {
               throw asiopq::Exception{*this};
            }
            continue;
         }
         if (not result)
         {
            continue;
         }
         co_yield ::std::move(result);
      }
      /// @note Need to wait for final result
      while (true)
      {
         if (::PQisBusy(conn) == 1)
            co_await wait_for_read_async();
         if (PQconsumeInput(conn) != 1)
         {
            throw asiopq::Exception{*this};
         }
         while (::PQisBusy(conn) == 0)
         {
            using ::PC::asiopq::ResultPtr;
            auto* const ptr = PQgetResult(conn);
            ResultPtr   result{ptr};
            if (not result)
            {
               co_return Result{};
            }
         }
      }
      co_return Result{};
   }

   int Connection::dup_native_socket_handle() const
   {
      auto const socket_pq_og = PQsocket(conn);
#ifdef _WIN32
      WSAPROTOCOL_INFOW info;
      if (WSADuplicateSocketW(socket_pq_og, GetProcessId(GetCurrentProcess()), &info) ==
          SOCKET_ERROR)
         throw std::system_error(std::error_code(GetLastError(), std::system_category()));

      auto n =
          WSASocketW(info.iAddressFamily, info.iSocketType, info.iProtocol, &info, 0, 0);
      if (n == INVALID_SOCKET)
         throw std::system_error(std::error_code(GetLastError(), std::system_category()));

      return n;
#else
      auto const socket_fd = ::dup(socket_pq_og);
      return socket_fd;
#endif
   }
} // namespace PC::asiopq
