#pragma once

#include <libpq-fe.h>

namespace PC::asiopq
{
    struct NotifyPtr
    {
    private:
        PGnotify *ptr;

    public:
        NotifyPtr(PGnotify *ptr = nullptr) noexcept;
        NotifyPtr(NotifyPtr &&other) noexcept;
        ~NotifyPtr();
        NotifyPtr& operator=(NotifyPtr &&other) noexcept;

        explicit operator bool() const noexcept
        {
            return ptr != nullptr;
        }

        PGnotify *operator*() noexcept
        {
            return ptr;
        }
        PGnotify *operator*() const noexcept
        {
            return ptr;
        }
        PGnotify *operator->() noexcept
        {
            return ptr;
        }
        PGnotify *operator->() const noexcept
        {
            return ptr;
        }
    };
}