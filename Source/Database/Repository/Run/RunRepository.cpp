#include <sqlite3.h>

#include <Api/KttException.h>
#include <Database/Repository/Run/RunRepository.h>
#include <Database/Repository/Utility.h>

namespace ktt
{

size_t RunRepository::CreateRun(sqlite3* connection, const size_t spaceId, const size_t architectureId)
{
    const char* runSQL = R"(
        INSERT INTO tuning_run (space_id, architecture_id)
        VALUES (?, ?)
    )";

    sqlite3_stmt* runStmt = DatabaseUtility::PrepareStatement(connection, runSQL, "Failed to prepare run INSERT statement: ");
    sqlite3_bind_int64(runStmt, 1, static_cast<sqlite3_int64>(spaceId));
    sqlite3_bind_int64(runStmt, 2, static_cast<sqlite3_int64>(architectureId));
    const int result = sqlite3_step(runStmt);

    if (result != SQLITE_DONE)
    {
        std::string error = sqlite3_errmsg(connection);
        sqlite3_finalize(runStmt);
        throw KttException("Failed to execute run INSERT statement: " + error, ExceptionReason::Database);
    }

    sqlite3_finalize(runStmt);
    return sqlite3_last_insert_rowid(connection);
}

} // namespace ktt

