#pragma once

#include <cstddef>
#include <sqlite3.h>

namespace ktt
{

class RunRepository
{
public:
    static size_t CreateRun(sqlite3* connection, size_t spaceId, size_t architectureId);
};

} // namespace ktt

