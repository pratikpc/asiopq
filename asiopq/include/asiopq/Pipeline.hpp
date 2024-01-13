// lib.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <asiopq/ResultPtr.hpp>
#include <boost/cobalt/generator.hpp>
#include <boost/cobalt/promise.hpp>
#include <libpq-fe.h>

namespace PC::asiopq
{
   struct Connection;
   struct Pipeline
   {
    private:
      Connection& connection;

    public:
      explicit Pipeline(decltype(connection) connection);
      ~Pipeline();

      ::PGpipelineStatus status() const;

      /// @note Changes the current connection into a Pipeline connection
      bool enter();

      /// @note This waits for the pipeline to complete
      ::boost::cobalt::generator<ResultPtr> async_wait();

      auto& conn() noexcept
      {
         return connection;
      }
      auto const& conn() const noexcept
      {
         return connection;
      }

      Connection* operator->()
      {
         auto& conn_ref = conn();
         return &conn_ref;
      }
      Connection const* operator->() const
      {
         auto& conn_ref = conn();
         return &conn_ref;
      }

    private:
   };
} // namespace PC::asiopq