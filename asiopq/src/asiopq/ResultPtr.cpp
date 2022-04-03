#include <asiopq/ResultPtr.hpp>
#include <cstdlib>
#include <utility>

namespace PC::asiopq
{
    ResultPtr::~ResultPtr()
    {
        if (ptr != nullptr)
        {
            PQclear(ptr);
        }
    }
    ResultPtr::ResultPtr(ResultPtr &&orig) : ptr{std::exchange(orig.ptr, nullptr)} {}

    ExecStatusType ResultPtr::status() const noexcept
    {
        return PQresultStatus(ptr);
    }
    int ResultPtr::field_number(std::string_view column) const noexcept
    {
        return PQfnumber(ptr, std::data(column));
    }
    std::string_view ResultPtr::field_name(int const column) const noexcept
    {
        return {PQfname(ptr, column)};
    }

    int ResultPtr::field_count() const noexcept
    {
        return PQnfields(ptr);
    }
    int ResultPtr::row_count() const noexcept
    {
        return PQntuples(ptr);
    }

    std::string_view ResultPtr::error_msg() const noexcept
    {
        return PQresultErrorMessage(ptr);
    }

    int ResultPtr::rows_affected() const noexcept
    {
        return std::atoi(PQcmdTuples(ptr));
    }
}