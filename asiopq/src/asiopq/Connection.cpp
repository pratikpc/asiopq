// lib.cpp : Defines the entry point for the application.
//

#include <asiopq/Connection.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/experimental/use_coro.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <stdexcept>
#include <utility>

namespace PC::asiopq
{
   Connection::Connection(decltype(executor) executor) :
       conn(nullptr), executor{::std::move(executor)}
   {
   }

   Connection::~Connection()
   {
      if (conn != nullptr)
         PQfinish(conn);
   }

   ::boost::asio::any_io_executor Connection::get_executor()
   {
      return executor;
   }

   boost::asio::experimental::coro<NotifyPtr>
       Connection::await_notify_async(::std::string_view command)
   {
      {
         auto const res = co_await command_async(command);
         if (not res)
         {
            co_return;
         }
      }
      auto op = await_notify_async();
      while (auto val = co_await op)
      {
         co_yield ::std::move(*val);
      }
   }
   boost::asio::experimental::coro<NotifyPtr> Connection::await_notify_async()
   {
      using boost::asio::experimental::use_coro;
      auto socket_pq = socket();
      while (true)
      {
         co_await socket_pq.async_read_some(::boost::asio::null_buffers(),
                                            ::boost::asio::deferred);
         if (PQconsumeInput(conn) != 1)
            continue;
         while (true)
         {
            auto result{PQnotifies(conn)};
            if (result == nullptr)
               break;
            if (PQconsumeInput(conn) != 1)
            {
               throw ::std::runtime_error("Unable to consume input");
            }
            co_yield result;
         }
      }
   }

   boost::asio::experimental::coro<ResultPtr>
       Connection::commands_async(std::string_view command)
   {
      // Returns 1 on success
      if (PQsendQuery(conn, std::data(command)) != 1)
      {
         co_return;
      }
      auto socket_pq = socket();
      while (true)
      {
         co_await socket_pq.async_read_some(::boost::asio::null_buffers(),
                                            boost::asio::deferred);
         if (PQconsumeInput(conn) != 1)
         {
            throw ::std::runtime_error("Unable to consume input");
         }
         while (::PQisBusy(conn) == 0)
         {
            ResultPtr result{PQgetResult(conn)};
            if (not result)
               co_return;
            co_yield ::std::move(result);
         }
      }
   }

   boost::asio::experimental::coro<void, ResultPtr>
       Connection::command_async(std::string_view command)
   {
      ResultPtr res;
      auto      op = commands_async(command);
      while (auto val = co_await op)
      {
         if (not *val)
         {
            continue;
         }
         res = ::std::move(*val);
      }
      co_return ::std::move(res);
   }
   void Connection::connect(std::string_view connection_string)
   {
      conn = PQconnectdb(std::data(connection_string));
   }

   asiopq_socket Connection::socket()
   {
      auto const socket_fd = dup_native_socket_handle();
      return asiopq_socket{get_executor(), {}, socket_fd};
   }

   PGconn* Connection::native_handle()
   {
      return conn;
   }

   boost::asio::experimental::coro<void>
       Connection::connect_async(std::string_view connection_string)
   {
      conn = PQconnectStart(std::data(connection_string));
      if (status() == ConnStatusType::CONNECTION_BAD)
         co_return;
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
                  throw ::std::runtime_error("Unable to switch to Non-Blocking mode");
               }
               co_return;
            }
            case PostgresPollingStatusType::PGRES_POLLING_FAILED:
            {
               if (::PQsetnonblocking(conn, 1) != 0)
               {
                  throw ::std::runtime_error("Unable to switch to Non-Blocking mode");
               }
               co_return;
            }
            // If Read, wait for PQ to read from Socket
            case PostgresPollingStatusType::PGRES_POLLING_READING:
            {
               auto socket_pq = socket();
               // Null buffers ensures we only wait
               // But do not end up reading anything
               co_await socket_pq.async_read_some(boost::asio::null_buffers{},
                                                  boost::asio::deferred);
            }
            break;
            // If Write, wait for PQ to write to socket
            case PostgresPollingStatusType::PGRES_POLLING_WRITING:
            {
               auto socket_pq = socket();
               // Null buffers ensures we only wait
               // But do not end up reading anything
               co_await socket_pq.async_write_some(boost::asio::null_buffers{},
                                                   boost::asio::deferred);
            }
            break;
         }
      }
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
