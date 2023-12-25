// lib.cpp : Defines the entry point for the application.
//

#include <asiopq/Connection.hpp>
#include <asiopq/NotifyPtr.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/cobalt/async_for.hpp>
#include <boost/cobalt/generator.hpp>
#include <boost/cobalt/op.hpp>
#include <boost/cobalt/promise.hpp>
#include <boost/cobalt/this_thread.hpp>
#include <stdexcept>
#include <utility>

namespace PC::asiopq
{

   namespace
   {
      inline asiopq_socket socket(int const socket_fd)
      {
         // auto const socket_fd = dup_native_socket_handle();
         return asiopq_socket{boost::cobalt::this_thread::get_executor(), {}, socket_fd};
      }
   } // namespace
   Connection::Connection() : conn(nullptr) {}

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
            throw ::std::invalid_argument("Unable to run the Notify command");
         }
      }
      auto op = await_notify_async();
      BOOST_COBALT_FOR(auto val, op)
      {
         co_yield ::std::move(val);
      }
      co_yield NotifyPtr{};
   }
   boost::cobalt::generator<NotifyPtr> Connection::await_notify_async()
   {
      auto socket_pq = socket(dup_native_socket_handle());
      while (true)
      {
         co_await socket_pq.async_read_some(::boost::asio::null_buffers(),
                                            ::boost::cobalt::use_op);
         if (PQconsumeInput(conn) != 1)
         {
            throw ::std::runtime_error("Unable to consume input");
         }
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
      co_yield NotifyPtr{};
   }

   boost::cobalt::generator<ResultPtr>
       Connection::commands_async(std::string_view command)
   {
      // Returns 1 on success
      if (PQsendQuery(conn, std::data(command)) != 1)
      {
         throw ::std::invalid_argument("Send Query failed");
      }
      auto socket_pq = socket(dup_native_socket_handle());
      while (true)
      {
         co_await socket_pq.async_read_some(::boost::asio::null_buffers(),
                                            boost::cobalt::use_op);
         if (PQconsumeInput(conn) != 1)
         {
            throw ::std::runtime_error("Unable to consume input");
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

   boost::cobalt::promise<void>
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
               auto socket_pq = socket(dup_native_socket_handle());
               // Null buffers ensures we only wait
               // But do not end up reading anything
               co_await socket_pq.async_read_some(boost::asio::null_buffers{},
                                                  boost::cobalt::use_op);
            }
            break;
            // If Write, wait for PQ to write to socket
            case PostgresPollingStatusType::PGRES_POLLING_WRITING:
            {
               auto socket_pq = socket(dup_native_socket_handle());
               // Null buffers ensures we only wait
               // But do not end up reading anything
               co_await socket_pq.async_write_some(boost::asio::null_buffers{},
                                                   boost::cobalt::use_op);
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
