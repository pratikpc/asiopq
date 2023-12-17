// lib.cpp : Defines the entry point for the application.
//

#include <asio/deferred.hpp>
#include <asiopq/Connection.hpp>
#include <asio/this_coro.hpp>
#include <asio/use_awaitable.hpp>
#include <asio/experimental/use_coro.hpp>
#include <stdexcept>

namespace PC::asiopq
{
   Connection::Connection() : conn(nullptr)
   {
   }
   Connection::~Connection()
   {
      if (conn != nullptr)
         PQfinish(conn);
   }
   asio::experimental::coro<NotifyPtr> Connection::await_notify_async(asio::any_io_executor& executor, ::std::string_view command)
   {
      {
         auto const res = co_await command_async(executor, command);
         if(not res)
         {
            co_return;
         }
      }
      auto op = await_notify_async(executor);
      while(auto val = co_await op)
      {
         co_yield ::std::move(*val);
      }
   }
   asio::experimental::coro<NotifyPtr> Connection::await_notify_async(asio::any_io_executor &executor)
   {
      using asio::experimental::use_coro;
      auto socket_pq = socket(executor);
      while (true)
      {
         co_await socket_pq.async_read_some(::asio::null_buffers(), ::asio::deferred);
         if (PQconsumeInput(conn) != 1)
            continue;
         while(true)
         {
            auto result{PQnotifies(conn)};
            if (result == nullptr)
               break;
            if(PQconsumeInput(conn) != 1)
            {
               throw ::std::runtime_error("Unable to consume input");
            }
            co_yield result;
         }
      }
   }

   asio::experimental::coro<ResultPtr> Connection::commands_async(asio::any_io_executor &executor, std::string_view command)
   {
      // Returns 1 on success
      if (PQsendQuery(conn, std::data(command)) != 1)
      {
         co_return;
      }
      auto socket_pq = socket(executor);
      while (true)
      {
         co_await socket_pq.async_read_some(::asio::null_buffers(), asio::deferred);
         if (PQconsumeInput(conn) != 1)
         {
            throw ::std::runtime_error("Unable to consume input");
         }
         while(::PQisBusy(conn) == 0)
         {
            ResultPtr result{PQgetResult(conn)};
            if (not result)
               co_return;
            co_yield ::std::move(result);
         }
      }
   }

   asio::experimental::coro<void, ResultPtr> Connection::command_async(asio::any_io_executor &executor, std::string_view command)
   {
      ResultPtr res;
      auto op = commands_async(executor, command);
      while(auto val = co_await op)
      {
         if(not *val)
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

   asiopq_socket Connection::socket(asio::any_io_executor &executor) const
   {
      auto const socket_fd = dup_native_socket_handle();
      return asiopq_socket{executor, {}, socket_fd};
   }

   PGconn *Connection::native_handle()
   {
      return conn;
   }

   asio::experimental::coro<void> Connection::connect_async(asio::any_io_executor &executor, std::string_view connection_string)
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
            if(::PQsetnonblocking(conn, 1) != 0)
            {
               throw ::std::runtime_error("Unable to switch to Non-Blocking mode");
            }
            co_return;
         }
         case PostgresPollingStatusType::PGRES_POLLING_FAILED:
         {
            if(::PQsetnonblocking(conn, 1) != 0)
            {
               throw ::std::runtime_error("Unable to switch to Non-Blocking mode");
            }
            co_return;
         }
         // If Read, wait for PQ to read from Socket
         case PostgresPollingStatusType::PGRES_POLLING_READING:
         {
            auto socket_pq = socket(executor);
            // Null buffers ensures we only wait
            // But do not end up reading anything
            co_await socket_pq.async_read_some(asio::null_buffers{}, asio::deferred);
         }
         break;
         // If Write, wait for PQ to write to socket
         case PostgresPollingStatusType::PGRES_POLLING_WRITING:
         {
            auto socket_pq = socket(executor);
            // Null buffers ensures we only wait
            // But do not end up reading anything
            co_await socket_pq.async_write_some(asio::null_buffers{}, asio::deferred);
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
      if (WSADuplicateSocketW(
              socket_pq_og,
              GetProcessId(GetCurrentProcess()),
              &info) == SOCKET_ERROR)
         throw std::system_error(
             std::error_code(
                 GetLastError(),
                 std::system_category()));

      auto n = WSASocketW(
          info.iAddressFamily,
          info.iSocketType,
          info.iProtocol,
          &info,
          0,
          0);
      if (n == INVALID_SOCKET)
         throw std::system_error(
             std::error_code(
                 GetLastError(),
                 std::system_category()));

      return n;
#else
      auto const socket_fd = ::dup(socket_pq_og);
      return socket_fd;
#endif
   }
}
