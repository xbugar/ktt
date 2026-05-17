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

} // namespace ktt::db
