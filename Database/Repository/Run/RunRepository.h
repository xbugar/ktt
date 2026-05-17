#pragma once

#include <cstddef>
#include <sqlite3.h>
#include <optional>

namespace ktt::db
{

struct Run
{
    std::optional<size_t> id;
    size_t spaceId;
    size_t deviceId;
    size_t deviceApiId;
    std::optional<std::string> inputData;
};

class RunRepository
{
public:
    static size_t CreateRun(sqlite3 *connection, const Run &run);
};

} // namespace ktt::db
