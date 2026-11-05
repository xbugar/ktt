#include <catch.hpp>

#if defined(KTT_DATABASE)

#include <optional>
#include <sqlite3.h>
#include <string>

#include "DatabaseTestHelpers.h"

using namespace ktt::db::test;

namespace
{

// Opens a private in-memory SQLite connection. The Database adopts it without owning it, so the test can inspect
// the stored rows directly through the same connection.
struct InMemoryConnection
{
    sqlite3 *handle = nullptr;

    InMemoryConnection()
    {
        sqlite3_open(":memory:", &handle);
    }

    ~InMemoryConnection()
    {
        sqlite3_close(handle);
    }
};

// Runs a query returning a single integer (e.g. a COUNT).
int64_t QueryInt(sqlite3 *connection, const std::string &sql)
{
    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare_v2(connection, sql.c_str(), -1, &stmt, nullptr);
    REQUIRE(sqlite3_step(stmt) == SQLITE_ROW);
    const int64_t value = sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);
    return value;
}

// Runs a query returning a single text value, or std::nullopt when the column is NULL.
std::optional<std::string> QueryOptionalText(sqlite3 *connection, const std::string &sql)
{
    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare_v2(connection, sql.c_str(), -1, &stmt, nullptr);
    REQUIRE(sqlite3_step(stmt) == SQLITE_ROW);

    std::optional<std::string> value;
    if (sqlite3_column_type(stmt, 0) != SQLITE_NULL)
        value = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));

    sqlite3_finalize(stmt);
    return value;
}

} // namespace

TEST_CASE("Database stores the source and space name of a new run", "Database")
{
    InMemoryConnection connection;
    const ktt::db::Database db(connection.handle);

    auto tuningInfo = MakeTuningInfo();
    tuningInfo.spaceInfo.sourceName = "My GEMM kernel";
    tuningInfo.spaceInfo.spaceName = "Default search space";

    db.SaveResults(tuningInfo, {MakeResult("simpleKernel", 64, 5 * Millisecond)});

    REQUIRE(QueryOptionalText(connection.handle, "SELECT name FROM tuning_source") == "My GEMM kernel");
    REQUIRE(QueryOptionalText(connection.handle, "SELECT name FROM tuning_space") == "Default search space");

    // The source name is also exposed through the public statistics API.
    const auto stats = db.GetStatsForSource(tuningInfo.spaceInfo.sourceFingerprint);
    REQUIRE(stats.has_value());
    REQUIRE(stats->name == "My GEMM kernel");
}

TEST_CASE("Database leaves the source and space name unset when none is given", "Database")
{
    InMemoryConnection connection;
    const ktt::db::Database db(connection.handle);

    const auto tuningInfo = MakeTuningInfo();
    db.SaveResults(tuningInfo, {MakeResult("simpleKernel", 64, 5 * Millisecond)});

    REQUIRE(QueryOptionalText(connection.handle, "SELECT name FROM tuning_source") == std::nullopt);
    REQUIRE(QueryOptionalText(connection.handle, "SELECT name FROM tuning_space") == std::nullopt);

    const auto stats = db.GetStatsForSource(tuningInfo.spaceInfo.sourceFingerprint);
    REQUIRE(stats.has_value());
    REQUIRE(stats->name == std::nullopt);
}

TEST_CASE("Database keeps the original name when the source and space already exist", "Database")
{
    InMemoryConnection connection;
    const ktt::db::Database db(connection.handle);

    auto first = MakeTuningInfo();
    first.spaceInfo.sourceName = "Original source name";
    first.spaceInfo.spaceName = "Original space name";
    db.SaveResults(first, {MakeResult("simpleKernel", 64, 5 * Millisecond)});

    // Same fingerprints, so the existing source and space rows are reused. A different, faster result keeps the
    // second save meaningful. The new names must be ignored: an existing source/space never has its name changed.
    auto second = MakeTuningInfo();
    second.spaceInfo.sourceName = "Renamed source";
    second.spaceInfo.spaceName = "Renamed space";
    db.SaveResults(second, {MakeResult("simpleKernel", 128, 4 * Millisecond)});

    REQUIRE(QueryInt(connection.handle, "SELECT COUNT(*) FROM tuning_source") == 1);
    REQUIRE(QueryInt(connection.handle, "SELECT COUNT(*) FROM tuning_space") == 1);
    REQUIRE(QueryOptionalText(connection.handle, "SELECT name FROM tuning_source") == "Original source name");
    REQUIRE(QueryOptionalText(connection.handle, "SELECT name FROM tuning_space") == "Original space name");
}

#endif // KTT_DATABASE
