#include <sqlite3.h>

#include <Api/KttException.h>
#include <Database/Repository/Source/SourceRepository.h>
#include <Database/Repository/Utility.h>
#include <Database/Schema/Mappers.h>
#include <Database/Schema/Udts/TuningSourceUdt.h>

namespace ktt
{

size_t SourceRepository::CreateSource(sqlite3 *connection, const TuningSourceSaveUdt &source)
{
    const char *sourceSQL = R"(
            INSERT INTO tuning_source (parameter_fingerprint, source_fingerprint, source)
            VALUES (?, ?, ?)
        )";

    auto sourceStmt = DatabaseUtility::PrepareStatement(
        connection,
        sourceSQL,
        "Failed to prepare source INSERT statement: "
    );

    sqlite3_bind_int64(sourceStmt, 1, static_cast<sqlite3_int64>(source.parameterFingerprint));
    sqlite3_bind_int64(sourceStmt, 2, static_cast<sqlite3_int64>(source.sourceFingerprint));

    if (source.source != nullptr)
    {
        sqlite3_bind_text(sourceStmt, 3, source.source->c_str(), -1, SQLITE_TRANSIENT);
    }
    else
    {
        sqlite3_bind_null(sourceStmt, 3);
    }

    const int result = sqlite3_step(sourceStmt);

    if (result != SQLITE_DONE)
    {
        std::string error = sqlite3_errmsg(connection);
        sqlite3_finalize(sourceStmt);
        throw KttException("Failed to execute source INSERT statement: " + error, ExceptionReason::Database);
    }

    sqlite3_finalize(sourceStmt);
    return static_cast<size_t>(sqlite3_last_insert_rowid(connection));
}


std::unique_ptr<TuningSourceDto> SourceRepository::SelectSourceByFingerprint(
    sqlite3 *connection, const size_t sourceFingerprint, const size_t parameterFingerprint
)
{
    const char *sourceSQL = R"(
            SELECT id, source_fingerprint, parameter_fingerprint, created_at
            FROM tuning_source
            WHERE source_fingerprint = ? AND parameter_fingerprint = ?
            LIMIT 1
        )";

    auto sourceStmt = DatabaseUtility::PrepareStatement(
        connection,
        sourceSQL,
        "Failed to prepare source SELECT statement: "
    );

    sqlite3_bind_int64(sourceStmt, 1, static_cast<sqlite3_int64>(sourceFingerprint));
    sqlite3_bind_int64(sourceStmt, 2, static_cast<sqlite3_int64>(parameterFingerprint));

    int result = sqlite3_step(sourceStmt);

    if (result == SQLITE_DONE)
    {
        sqlite3_finalize(sourceStmt);
        return nullptr;
    }

    if (result != SQLITE_ROW)
    {
        std::string error = sqlite3_errmsg(connection);
        sqlite3_finalize(sourceStmt);
        throw KttException("Failed to execute source SELECT statement: " + error, ExceptionReason::Database);
    }

    TuningSourceDto source;
    {
        source.id = sqlite3_column_int64(sourceStmt, 0);
        source.sourceFingerprint = sqlite3_column_int64(sourceStmt, 1);
        source.parameterFingerprint = sqlite3_column_int64(sourceStmt, 2);
        source.createdAt = DatabaseUtility::Sqlite3ColumnString(sourceStmt, 3);
    }

    sqlite3_finalize(sourceStmt);

    return std::make_unique<TuningSourceDto>(source);
}
} // namespace ktt
