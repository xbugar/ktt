#pragma once

#include <cstddef>
#include <sqlite3.h>

namespace ktt
{

class RunRepository
{
public:
    static size_t CreateRun(sqlite3* connection);
};

} // namespace ktt

