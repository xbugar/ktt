#pragma once

#include <memory>
#include <optional>
#include <sqlite3.h>

namespace ktt::db
{
struct Source
{
    std::optional<size_t> id;
    size_t sourceFingerprint;
};

class SourceRepository
{
public:
    static std::optional<Source> GetSourceByFingerprint(sqlite3 *connection, size_t sourceFingerprint);

    static Source CreateSource(sqlite3 *connection, const Source &source);

    static Source GetOrCreateSource(sqlite3 *connection, Source source);
};

} // namespace ktt::db
