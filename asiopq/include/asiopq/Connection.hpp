// lib.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <boost/asio/local/stream_protocol.hpp>
#include <boost/cobalt/generator.hpp>
#include <boost/cobalt/promise.hpp>
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
      PGconn* conn;

    public:
      Connection();
      ~Connection();

      void                         connect(std::string_view connection_string);
      boost::cobalt::promise<void> connect_async(std::string_view connection_string);

      boost::cobalt::generator<ResultPtr> commands_async(std::string_view command);
      boost::cobalt::promise<ResultPtr>   command_async(std::string_view command);

      boost::cobalt::generator<NotifyPtr> await_notify_async();
      boost::cobalt::generator<NotifyPtr> await_notify_async(::std::string_view command);

      ConnStatusType status() const
      {
         return PQstatus(conn);
      }
      std::string_view error_msg() const
      {
         return PQerrorMessage(conn);
      }
      PGconn* native_handle();

    private:
      int dup_native_socket_handle() const;
   };
} // namespace PC::asiopq