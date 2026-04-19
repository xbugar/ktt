#pragma once
#include <cstddef>
#include <string>
#include <sqlite3.h>

namespace ktt
{
struct TuningSpaceSaveUdt
{
    std::size_t sourceId;
    std::size_t spaceFingerprint;
};

struct TuningSpaceLoadUdt
{
    std::size_t id{};
    std::size_t sourceId{};
    std::size_t spaceFingerprint{};
    std::string createdAt;
    static TuningSpaceLoadUdt FromRow(sqlite3_stmt *statement);
};

} // namespace ktt

