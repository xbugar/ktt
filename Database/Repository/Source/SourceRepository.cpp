#include <sqlite3.h>
#include <string>

#include <Api/KttException.h>
#include <Repository/Source/SourceRepository.h>
#include <Repository/Utility.h>

namespace ktt::db
{

Source SourceRepository::CreateSource(sqlite3 *connection, const Source &source)
{
    const char *sourceSQL = R"(
            INSERT INTO tuning_source (source_fingerprint)
            VALUES (?)
        )";

    auto sourceStmt = DatabaseUtility::PrepareStatement(
        connection,
        sourceSQL,
        "Failed to prepare source INSERT statement: "
    );

    const auto fingerprintText = std::to_string(source.sourceFingerprint);
    sqlite3_bind_text(sourceStmt, 1, fingerprintText.c_str(), -1, SQLITE_TRANSIENT);

    const int result = sqlite3_step(sourceStmt);

    if (result != SQLITE_DONE)
    {
        std::string error = sqlite3_errmsg(connection);
        sqlite3_finalize(sourceStmt);
        throw KttException("Failed to execute source INSERT statement: " + error);
    }

    const size_t sourceId = static_cast<size_t>(sqlite3_last_insert_rowid(connection));
    sqlite3_finalize(sourceStmt);
    return Source{sourceId, source.sourceFingerprint};
}


std::optional<Source> SourceRepository::GetSourceByFingerprint(sqlite3 *connection, const size_t sourceFingerprint)
{
    const char *sourceSQL = R"(
            SELECT id, source_fingerprint
            FROM tuning_source
            WHERE source_fingerprint = ?
            LIMIT 1
        )";

    auto sourceStmt = DatabaseUtility::PrepareStatement(
        connection,
        sourceSQL,
        "Failed to prepare source SELECT statement: "
    );

    const auto fingerprintText = std::to_string(sourceFingerprint);
    sqlite3_bind_text(sourceStmt, 1, fingerprintText.c_str(), -1, SQLITE_TRANSIENT);

    int result = sqlite3_step(sourceStmt);

    if (result == SQLITE_DONE)
    {
        sqlite3_finalize(sourceStmt);
        return std::nullopt;
    }

    if (result != SQLITE_ROW)
    {
        std::string error = sqlite3_errmsg(connection);
        sqlite3_finalize(sourceStmt);
        throw KttException("Failed to execute source SELECT statement: " + error);
    }

    Source source;
    {
        source.id = sqlite3_column_int64(sourceStmt, 0);
        const auto fingerprintText = DatabaseUtility::ReadTextColumn(sourceStmt, 1);
        source.sourceFingerprint = static_cast<size_t>(std::stoull(fingerprintText));
    }

    sqlite3_finalize(sourceStmt);

    return source;
}

Source SourceRepository::GetOrCreateSource(sqlite3 *connection, Source source)
{
    if (auto existingSource = GetSourceByFingerprint(connection, source.sourceFingerprint))
        return *existingSource;

    return CreateSource(connection, source);
}

std::optional<SourceStats> SourceRepository::GetStatsForSource(sqlite3 *connection, const size_t sourceFingerprint)
{
    const char *statsSql = R"(
        SELECT
            (SELECT COUNT(*) FROM tuning_space WHERE source_id = tuning_source.id) AS space_count,
            (SELECT COUNT(DISTINCT tuning_run.device_id)
                FROM tuning_run
                JOIN tuning_space ON tuning_space.id = tuning_run.space_id
                WHERE tuning_space.source_id = tuning_source.id) AS device_count,
            (SELECT COUNT(*)
                FROM tuning_run
                JOIN tuning_space ON tuning_space.id = tuning_run.space_id
                WHERE tuning_space.source_id = tuning_source.id) AS run_count,
            (SELECT COUNT(*)
                FROM tuning_result
                JOIN tuning_run ON tuning_run.id = tuning_result.run_id
                JOIN tuning_space ON tuning_space.id = tuning_run.space_id
                WHERE tuning_space.source_id = tuning_source.id) AS result_count
        FROM tuning_source
        WHERE source_fingerprint = ?
        LIMIT 1
    )";

    sqlite3_stmt *statsStmt = DatabaseUtility::PrepareStatement(
        connection,
        statsSql,
        "Failed to prepare source stats SELECT statement: "
    );

    const auto fingerprintText = std::to_string(sourceFingerprint);
    sqlite3_bind_text(statsStmt, 1, fingerprintText.c_str(), -1, SQLITE_TRANSIENT);

    const int result = sqlite3_step(statsStmt);

    if (result == SQLITE_DONE)
    {
        sqlite3_finalize(statsStmt);
        return std::nullopt;
    }

    if (result != SQLITE_ROW)
    {
        std::string error = sqlite3_errmsg(connection);
        sqlite3_finalize(statsStmt);
        throw KttException("Failed to execute source stats SELECT statement: " + error);
    }

    SourceStats stats{};
    stats.spaceCount = static_cast<size_t>(sqlite3_column_int64(statsStmt, 0));
    stats.deviceCount = static_cast<size_t>(sqlite3_column_int64(statsStmt, 1));
    stats.runCount = static_cast<size_t>(sqlite3_column_int64(statsStmt, 2));
    stats.resultCount = static_cast<size_t>(sqlite3_column_int64(statsStmt, 3));

    sqlite3_finalize(statsStmt);
    return stats;
}

} // namespace ktt::db
