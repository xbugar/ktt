#include <sqlite3.h>

#include <Api/KttException.h>
#include <Database/Repository/Space/SpaceRepository.h>
#include <Database/Schema/Mappers.h>
#include <Database/Repository/Utility.h>

namespace ktt
{


std::unique_ptr<TuningSpaceLoadUdt> SpaceRepository::SelectSpaceByFingerprint(sqlite3* connection, const size_t sourceId,
    const size_t spaceFingerprint)
{
    const char* spaceSQL = R"(
        SELECT id, source_id, space_fingerprint, created_at
        FROM tuning_space
        WHERE source_id = ? AND space_fingerprint = ?
        LIMIT 1
    )";

    sqlite3_stmt* spaceStmt = DatabaseUtility::PrepareStatement(connection, spaceSQL, "Failed to prepare space SELECT statement: ");
    sqlite3_bind_int64(spaceStmt, 1, static_cast<sqlite3_int64>(sourceId));
    sqlite3_bind_int64(spaceStmt, 2, static_cast<sqlite3_int64>(spaceFingerprint));

    const int result = sqlite3_step(spaceStmt);

    if (result == SQLITE_DONE)
    {
        sqlite3_finalize(spaceStmt);
        return nullptr;
    }

    if (result != SQLITE_ROW)
    {
        std::string error = sqlite3_errmsg(connection);
        sqlite3_finalize(spaceStmt);
        throw KttException("Failed to execute space SELECT statement: " + error, ExceptionReason::Database);
    }

    auto output = std::make_unique<TuningSpaceLoadUdt>(TuningSpaceLoadUdt::FromRow(spaceStmt));
    sqlite3_finalize(spaceStmt);

    return output;
}

size_t SpaceRepository::CreateSpace(sqlite3* connection, const TuningSpaceSaveUdt& space)
{
    const char* spaceSQL = R"(
        INSERT INTO tuning_space (source_id, space_fingerprint)
        VALUES (?, ?)
    )";

    sqlite3_stmt* spaceStmt = DatabaseUtility::PrepareStatement(connection, spaceSQL, "Failed to prepare space INSERT statement: ");
    sqlite3_bind_int64(spaceStmt, 1, static_cast<sqlite3_int64>(space.sourceId));
    sqlite3_bind_int64(spaceStmt, 2, static_cast<sqlite3_int64>(space.spaceFingerprint));

    const int result = sqlite3_step(spaceStmt);

    if (result != SQLITE_DONE)
    {
        std::string error = sqlite3_errmsg(connection);
        sqlite3_finalize(spaceStmt);
        throw KttException("Failed to execute space INSERT statement: " + error, ExceptionReason::Database);
    }

    sqlite3_finalize(spaceStmt);
    return static_cast<size_t>(sqlite3_last_insert_rowid(connection));
}

} // namespace ktt

