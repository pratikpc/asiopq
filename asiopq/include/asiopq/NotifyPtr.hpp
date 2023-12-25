#pragma once

#include <asiopq/PQMemory.hpp>
#include <libpq-fe.h>

namespace PC::asiopq
{
   using NotifyPtr = PQMemory<::PGnotify>;
} // namespace PC::asiopq