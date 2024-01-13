#include <asiopq/Connection.hpp>
#include <asiopq/Exception.hpp>
#include <asiopq/Pipeline.hpp>
#include <asiopq/ResultPtr.hpp>
#include <boost/cobalt/generator.hpp>
#include <boost/cobalt/op.hpp>
#include <libpq-fe.h>

namespace PC::asiopq
{
   Pipeline::Pipeline(decltype(connection) connection) : connection{connection} {}
   Pipeline::~Pipeline()
   {
      ::PQexitPipelineMode(connection.native_handle());
   }
   bool Pipeline::enter()
   {
      return ::PQenterPipelineMode(connection.native_handle()) == 1;
   }

   ::PGpipelineStatus Pipeline::status() const
   {
      return ::PQpipelineStatus(connection.native_handle());
   }

   ::boost::cobalt::generator<ResultPtr> Pipeline::async_wait()
   {
      if (auto const res = ::PQpipelineSync(connection.native_handle()); res != 1)
      {
         throw asiopq::Exception{*this};
      }
      bool continue_to_loop = true;
      while (continue_to_loop)
      {
         co_await (*this)->wait_for_read_async();
         if (PQconsumeInput((*this)->native_handle()) != 1)
         {
            throw asiopq::Exception{*this};
         }
         while (::PQisBusy((*this)->native_handle()) == 0)
         {
            auto* const ptr = PQgetResult((*this)->native_handle());
            if (ptr == nullptr)
            {
               if (not continue_to_loop)
                  break;
               continue;
            }
            ResultPtr result{ptr};
            if (result.status() == PGRES_PIPELINE_SYNC)
            {
               /// @note We know that the next result is going to be a nullptr
               /// @note And this is the final result we actually need
               continue_to_loop = false;
               continue;
            }
            co_yield ::std::move(result);
         }
      }
      co_return ResultPtr{nullptr};
   }
} // namespace PC::asiopq