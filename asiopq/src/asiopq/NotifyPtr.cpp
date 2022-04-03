#include <asiopq/NotifyPtr.hpp>
#include <utility>

namespace PC::asiopq
{
    NotifyPtr::NotifyPtr(PGnotify *ptr) noexcept : ptr{ptr}
    {
    }
    NotifyPtr::NotifyPtr(NotifyPtr &&other) noexcept : ptr{std::exchange(other.ptr, nullptr)} {}
    NotifyPtr &NotifyPtr::operator=(NotifyPtr &&other) noexcept
    {
        ptr = std::exchange(other.ptr, nullptr);
        return *this;
    }

    NotifyPtr::~NotifyPtr()
    {
        if (ptr != nullptr)
            PQfreemem(ptr);
    }
}
