// lib.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <libpq-fe.h>
#include <asio/awaitable.hpp>
#include <asio/any_io_executor.hpp>
#include <asio/local/stream_protocol.hpp>
#include <asio/experimental/coro.hpp>
#include <string_view>

#include <asiopq/ResultPtr.hpp>
#include <asiopq/NotifyPtr.hpp>

namespace PC::asiopq
{
   using ResultsPtr = std::vector<ResultPtr>;
   namespace
   {
      using asio::local::stream_protocol;
      using asiopq_socket = stream_protocol::socket;
   }
   struct Connection
   {
   private:
      PGconn *conn;

   public:
      Connection();
      ~Connection();

      void connect(std::string_view connection_string);
      asio::awaitable<void> connect_async(std::string_view connection_string);

      asio::awaitable<ResultsPtr> commands_async(std::string_view command);
      asio::awaitable<ResultPtr> command_async(std::string_view command);

      asio::experimental::coro<NotifyPtr> await_notify_async(asio::any_io_executor& executor);

      ConnStatusType status() const
      {
         return PQstatus(conn);
      }
      std::string_view error_msg() const
      {
         return PQerrorMessage(conn);
      }
      PGconn *native_handle();
      asiopq_socket socket(asio::any_io_executor &executor) const;

   private:
      int dup_native_socket_handle() const;
   };
}