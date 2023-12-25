#pragma once

#include <libpq-fe.h>
#include <utility>

namespace PC::asiopq
{
   template <typename T>
   struct PQMemory
   {
    private:
      T* ptr;

    public:
      PQMemory(T* ptr = nullptr) noexcept;
      PQMemory(PQMemory&& other) noexcept;
      ~PQMemory();
      PQMemory& operator=(PQMemory&& other) noexcept;

      explicit operator bool() const noexcept
      {
         return ptr != nullptr;
      }
      T** ref_ptr()
      {
         return &ptr;
      }

      T* operator*() noexcept
      {
         return ptr;
      }
      T* operator*() const noexcept
      {
         return ptr;
      }
      T* operator->() noexcept
      {
         return ptr;
      }
      T* operator->() const noexcept
      {
         return ptr;
      }
   };
   template <typename T>
   PQMemory<T>::PQMemory(T* ptr) noexcept : ptr{ptr}
   {
   }
   template <typename T>
   PQMemory<T>::PQMemory(PQMemory<T>&& other) noexcept :
       ptr{std::exchange(other.ptr, nullptr)}
   {
   }
   template <typename T>
   PQMemory<T>& PQMemory<T>::operator=(PQMemory&& other) noexcept
   {
      ptr = std::exchange(other.ptr, nullptr);
      return *this;
   }
   template <typename T>
   PQMemory<T>::~PQMemory()
   {
      if (ptr != nullptr)
         PQfreemem(ptr);
   }
} // namespace PC::asiopq