#include <asiopq/ValuePtr.hpp>

#include <cstdlib>

#include <libpq-fe.h>

namespace PC::asiopq
{
   std::string_view ValuePtr::field_name() const noexcept
   {
      return {PQfname(ptr, col)};
   }
   int ValuePtr::length() const noexcept
   {
      return PQgetlength(ptr, row, col);
   }
   bool ValuePtr::is_null() const noexcept
   {
      return PQgetisnull(ptr, row, col) == 1;
   }
   bool ValuePtr::is_binary() const noexcept
   {
      return PQbinaryTuples(ptr) == 1;
   }
   template <>
   std::string_view ValuePtr::as<std::string_view>() const noexcept
   {
      return std::string_view{PQgetvalue(ptr, row, col)};
   }
   template <>
   int ValuePtr::as<int>() const noexcept
   {
      auto value_str = as<std::string_view>();
      return std::atoi(std::data(value_str));
   }
   template <>
   long ValuePtr::as<long>() const noexcept
   {
      auto value_str = as<std::string_view>();
      return std::atol(std::data(value_str));
   }
   template <>
   long long ValuePtr::as<long long>() const noexcept
   {
      auto value_str = as<std::string_view>();
      return std::atoll(std::data(value_str));
   }

} // namespace PC::asiopq