#pragma once

#include <memory>
#include <optional>
#include <sqlite3.h>

namespace ktt::db
{

struct Space
{
    std::optional<size_t> id;
    size_t sourceId;
    size_t parameterFingerprint;
    size_t spaceFingerprint;
};

class SpaceRepository
{
public:
    static std::optional<Space> GetSpace(sqlite3 *connection, Space space);

    static Space CreateSpace(sqlite3 *connection, Space space);

    static Space GetOrCreateSpace(sqlite3 *connection, Space space);
};

} // namespace ktt::db
