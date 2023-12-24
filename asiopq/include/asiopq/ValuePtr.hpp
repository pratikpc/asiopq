#pragma once

#include <cstdlib>
#include <libpq-fe.h>
#include <string_view>

namespace PC::asiopq
{
   struct ValuePtr
   {
    private:
      PGresult* ptr;
      int const row;
      int const col;

    public:
      ValuePtr(PGresult* const ptr, int const col, int const row = 0) :
          ptr{ptr}, row(row), col(col)
      {
      }

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
      std::string_view field_name() const noexcept;
      int              length() const noexcept;

      bool is_null() const noexcept;
      bool is_binary() const noexcept;

      template <typename T>
      T as() const noexcept;
   };

   template <>
   std::string_view ValuePtr::as<std::string_view>() const noexcept;
   template <>
   int ValuePtr::as<int>() const noexcept;
   template <>
   long ValuePtr::as<long>() const noexcept;
   template <>
   long long ValuePtr::as<long long>() const noexcept;
} // namespace PC::asiopq
