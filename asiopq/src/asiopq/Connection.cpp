// lib.cpp : Defines the entry point for the application.
//

#include <asiopq/Connection.hpp>
#include <asio/this_coro.hpp>
#include <asio/use_awaitable.hpp>
#include <asio/experimental/use_coro.hpp>

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
   asio::experimental::coro<NotifyPtr> Connection::await_notify_async(asio::any_io_executor &executor)
   {
      using asio::experimental::use_coro;
      auto socket_pq = socket(executor);
      while (true)
      {
         if (PQisBusy(conn) == 1)
            co_await socket_pq.async_read_some(::asio::null_buffers(), use_coro);
         if (PQconsumeInput(conn) != 1)
            continue;
         auto result{PQnotifies(conn)};
         if (result != nullptr)
            co_yield result;
      }
   }

   asio::awaitable<ResultsPtr> Connection::commands_async(std::string_view command)
   {
      // Returns 1 on success
      if (PQsendQuery(conn, std::data(command)) != 1)
         co_return ResultsPtr{};
      auto executor = co_await ::asio::this_coro::executor;
      auto socket_pq = socket(executor);
      ResultsPtr results;
      while (true)
      {
         if (PQisBusy(conn) == 1)
            co_await socket_pq.async_read_some(::asio::null_buffers(), asio::use_awaitable);
         if (PQconsumeInput(conn) != 1)
            co_return ResultsPtr{};
         ResultPtr result{PQgetResult(conn)};
         if (not result)
            break;
         results.push_back(std::move(result));
      }

      co_return results;
   }

   asio::awaitable<ResultPtr> Connection::command_async(std::string_view command)
   {
      ResultsPtr results = co_await commands_async(command);
      if (std::empty(results))
         co_return ResultPtr{};
      ResultPtr result = std::move(results[0]);
      results.clear();
      co_return result;
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

   asio::awaitable<void> Connection::connect_async(std::string_view connection_string)
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
            co_return;
         }
         case PostgresPollingStatusType::PGRES_POLLING_FAILED:
         {
            co_return;
         }
         // If Read, wait for PQ to read from Socket
         case PostgresPollingStatusType::PGRES_POLLING_READING:
         {
            auto executor = co_await asio::this_coro::executor;
            auto socket_pq = socket(executor);
            // Null buffers ensures we only wait
            // But do not end up reading anything
            co_await socket_pq.async_read_some(asio::null_buffers{}, asio::use_awaitable);
         }
         break;
         // If Write, wait for PQ to write to socket
         case PostgresPollingStatusType::PGRES_POLLING_WRITING:
         {
            auto executor = co_await asio::this_coro::executor;
            auto socket_pq = socket(executor);
            // Null buffers ensures we only wait
            // But do not end up reading anything
            co_await socket_pq.async_write_some(asio::null_buffers{}, asio::use_awaitable);
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
