#include <sqlite3.h>
#include <string>

#include <Api/KttException.h>
#include <Database/Repository/Space/SpaceRepository.h>
#include <Database/Repository/Utility.h>

namespace ktt::db
{

std::optional<Space> SpaceRepository::GetSpace(sqlite3* connection, Space space)
{
    const char* spaceSQL = R"(
        SELECT id, source_id, space_fingerprint, parameter_fingerprint
        FROM tuning_space
        WHERE source_id = ? AND space_fingerprint = ? AND parameter_fingerprint = ?
        LIMIT 1
    )";

    sqlite3_stmt* spaceStmt = DatabaseUtility::PrepareStatement(
        connection,
        spaceSQL,
        "Failed to prepare space SELECT statement: "
    );
    sqlite3_bind_int64(spaceStmt, 1, static_cast<sqlite3_int64>(space.sourceId));
    const auto spaceFingerprintText = std::to_string(space.spaceFingerprint);
    const auto parameterFingerprintText = std::to_string(space.parameterFingerprint);
    sqlite3_bind_text(spaceStmt, 2, spaceFingerprintText.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(spaceStmt, 3, parameterFingerprintText.c_str(), -1, SQLITE_TRANSIENT);

    const int result = sqlite3_step(spaceStmt);

    if (result == SQLITE_DONE)
    {
        sqlite3_finalize(spaceStmt);
        return std::nullopt;
    }

    if (result != SQLITE_ROW)
    {
        std::string error = sqlite3_errmsg(connection);
        sqlite3_finalize(spaceStmt);
        throw KttException("Failed to execute space SELECT statement: " + error);
    }

    Space spaceResult;
    {
        spaceResult.id = sqlite3_column_int64(spaceStmt, 0);
        spaceResult.sourceId = sqlite3_column_int64(spaceStmt, 1);
        const auto spaceFingerprintText = DatabaseUtility::ReadTextColumn(spaceStmt, 2);
        const auto parameterFingerprintText = DatabaseUtility::ReadTextColumn(spaceStmt, 3);
        spaceResult.spaceFingerprint = static_cast<size_t>(std::stoull(spaceFingerprintText));
        spaceResult.parameterFingerprint = static_cast<size_t>(std::stoull(parameterFingerprintText));
    }
    sqlite3_finalize(spaceStmt);

    return spaceResult;
}

Space SpaceRepository::CreateSpace(sqlite3* connection, Space space)
{
    const char* spaceSQL = R"(
        INSERT INTO tuning_space (source_id, space_fingerprint, parameter_fingerprint)
        VALUES (?, ?, ?)
    )";

    sqlite3_stmt* spaceStmt = DatabaseUtility::PrepareStatement(
        connection,
        spaceSQL,
        "Failed to prepare space INSERT statement: "
    );
    sqlite3_bind_int64(spaceStmt, 1, static_cast<sqlite3_int64>(space.sourceId));
    const auto spaceFingerprintText = std::to_string(space.spaceFingerprint);
    const auto parameterFingerprintText = std::to_string(space.parameterFingerprint);
    sqlite3_bind_text(spaceStmt, 2, spaceFingerprintText.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(spaceStmt, 3, parameterFingerprintText.c_str(), -1, SQLITE_TRANSIENT);
    const int result = sqlite3_step(spaceStmt);

    if (result != SQLITE_DONE)
    {
        std::string error = sqlite3_errmsg(connection);
        sqlite3_finalize(spaceStmt);
        throw KttException("Failed to execute space INSERT statement: " + error);
    }

    sqlite3_finalize(spaceStmt);

    space.id = static_cast<size_t>(sqlite3_last_insert_rowid(connection));
    return space;
}

Space SpaceRepository::GetOrCreateSpace(sqlite3* connection, Space space)
{
    if (auto existing = GetSpace(connection, space))
        return *existing;

    return CreateSpace(connection, space);
}

} // namespace ktt::db
