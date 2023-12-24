#pragma once

#include <asiopq/ValuePtr.hpp>
#include <libpq-fe.h>
#include <string_view>

namespace PC::asiopq
{
   struct ResultPtr
   {
    private:
      PGresult* ptr;

    public:
      explicit ResultPtr(PGresult* const ptr = nullptr) : ptr{ptr} {}
      ResultPtr(ResultPtr&& orig);
      ResultPtr& operator=(ResultPtr&& orig);
      ~ResultPtr();

      explicit operator bool() const noexcept
      {
         return ptr != nullptr;
      }
      PGresult* operator*() noexcept
      {
         return ptr;
      }
      PGresult* operator*() const noexcept
      {
         return ptr;
      }
      PGresult* operator->() noexcept
      {
         return ptr;
      }
      PGresult* operator->() const noexcept
      {
         return ptr;
      }
      int              field_number(std::string_view column) const noexcept;
      std::string_view field_name(int const column) const noexcept;
      int              field_count() const noexcept;
      int              row_count() const noexcept;

      int rows_affected() const noexcept;

      ExecStatusType status() const noexcept;

      std::string_view error_msg() const noexcept;

      ValuePtr operator()(int const col, int const row = 0) const noexcept
      {
         return {ptr, col, row};
      }
   };
} // namespace PC::asiopq
