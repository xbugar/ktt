#include <sqlite3.h>

#include <Api/KttException.h>
#include <Database/Repository/Run/RunRepository.h>

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

size_t RunRepository::CreateRun(sqlite3* connection)
{
    const char* runSQL = R"(
        INSERT INTO tuning_run DEFAULT VALUES
    )";

    sqlite3_stmt* runStmt = PrepareStatement(connection, runSQL, "Failed to prepare run INSERT statement: ");
    const int result = sqlite3_step(runStmt);

    if (result != SQLITE_DONE)
    {
        std::string error = sqlite3_errmsg(connection);
        sqlite3_finalize(runStmt);
        throw KttException("Failed to execute run INSERT statement: " + error, ExceptionReason::Database);
    }

    sqlite3_finalize(runStmt);
    return static_cast<size_t>(sqlite3_last_insert_rowid(connection));
}

} // namespace ktt

