#pragma once

#include <cstddef>
#include <optional>
#include <sqlite3.h>
#include <vector>

#include <Api/Info/DatabaseTuningInfo.h>

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

struct RunQueryResult
{
    size_t runId{};
    std::optional<std::string> inputData;
    DeviceInfo deviceInfo{};
};

class RunRepository
{
public:
    static size_t CreateRun(sqlite3 *connection, const Run &run);
    static std::vector<RunQueryResult> GetRunsForSpace(sqlite3 *connection, size_t spaceId);
    static std::vector<RunQueryResult> GetRunsForSpacePaged(
        sqlite3 *connection, size_t spaceId, size_t offset, size_t limit
    );
};

} // namespace ktt::db
