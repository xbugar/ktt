#include <sqlite3.h>

#include <Api/KttException.h>
#include <Database/Repository/Source/SourceRepository.h>
#include <Database/Schema/Mappers.h>
#include <Database/Schema/Udts/TuningSourceUdt.h>
#include <Utility/Logger/Logger.h>


namespace ktt
{

namespace
{

sqlite3_stmt* PrepareStatement(sqlite3* connection, const char* sql, const char* errorPrefix)
{
    sqlite3_stmt* statement = nullptr;
    const int result = sqlite3_prepare_v2(connection, sql, -1, &statement, nullptr);

    if (result != SQLITE_OK)
    {
        throw KttException(std::string(errorPrefix) + sqlite3_errmsg(connection), ExceptionReason::Database);
    }

    return statement;
}

} // namespace

size_t SourceRepository::CreateSource(sqlite3* connection, const TuningSourceSaveUdt& source)
{
    const char* sourceSQL = R"(
        INSERT INTO tuning_source (parameter_fingerprint, source_fingerprint)
        VALUES (?, ?)
    )";

    sqlite3_stmt* sourceStmt = PrepareStatement(connection, sourceSQL, "Failed to prepare source INSERT statement: ");
    sqlite3_bind_int64(sourceStmt, 1, static_cast<sqlite3_int64>(source.parameterFingerprint));
    sqlite3_bind_int64(sourceStmt, 2, static_cast<sqlite3_int64>(source.sourceFingerprint));

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


std::unique_ptr<TuningSourceDto> SourceRepository::SelectSourceByFingerprints(
    sqlite3* connection,
    const size_t sourceFingerprint,
    const size_t parameterFingerprint)
{
    const char* sourceSQL = R"(
        SELECT id, source_fingerprint, parameter_fingerprint, created_at
        FROM tuning_source
        WHERE parameter_fingerprint = ? AND source_fingerprint = ?
        LIMIT 1
    )";

    sqlite3_stmt* sourceStmt = PrepareStatement(connection, sourceSQL, "Failed to prepare source SELECT statement: ");
    sqlite3_bind_int64(sourceStmt, 1, static_cast<sqlite3_int64>(parameterFingerprint));
    sqlite3_bind_int64(sourceStmt, 2, static_cast<sqlite3_int64>(sourceFingerprint));

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

    auto output = std::make_unique<TuningSourceDto>(Mappers::MapSourceLoadRow(sourceStmt));
    sqlite3_finalize(sourceStmt);

    return output;
}
} // namespace ktt
