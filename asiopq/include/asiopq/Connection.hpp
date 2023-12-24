﻿// lib.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/experimental/coro.hpp>
#include <boost/asio/local/stream_protocol.hpp>
#include <libpq-fe.h>
#include <string_view>

#include <asiopq/NotifyPtr.hpp>
#include <asiopq/ResultPtr.hpp>

namespace PC::asiopq
{
   using ResultsPtr = std::vector<ResultPtr>;
   namespace
   {
      using boost::asio::local::stream_protocol;
      using asiopq_socket = stream_protocol::socket;
   } // namespace
   struct Connection
   {
    private:
      PGconn*                        conn;
      ::boost::asio::any_io_executor executor;

    public:
      explicit Connection(decltype(executor) executor);
      ~Connection();

      ::boost::asio::any_io_executor get_executor();

      void connect(std::string_view connection_string);
      boost::asio::experimental::coro<void>
          connect_async(std::string_view connection_string);

      boost::asio::experimental::coro<ResultPtr> commands_async(std::string_view command);
      boost::asio::experimental::coro<void, ResultPtr>
          command_async(std::string_view command);

      boost::asio::experimental::coro<NotifyPtr> await_notify_async();
      boost::asio::experimental::coro<NotifyPtr>
          await_notify_async(::std::string_view command);

      ConnStatusType status() const
      {
         return PQstatus(conn);
      }
      std::string_view error_msg() const
      {
         return PQerrorMessage(conn);
      }
      PGconn*       native_handle();
      asiopq_socket socket();

    private:
      int dup_native_socket_handle() const;
   };
} // namespace PC::asiopq