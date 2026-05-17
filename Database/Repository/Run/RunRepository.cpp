#include <sqlite3.h>

#include <Api/KttException.h>
#include <Repository/Run/RunRepository.h>
#include <Repository/Utility.h>

namespace ktt::db
{

size_t RunRepository::CreateRun(
    sqlite3 *connection, const Run &run
)
{
    const char *runSQL = R"(
        INSERT INTO tuning_run (space_id, device_id, device_api_id, input_data)
        VALUES (?, ?, ?, ?)
    )";

    sqlite3_stmt *runStmt = DatabaseUtility::PrepareStatement(
        connection,
        runSQL,
        "Failed to prepare run INSERT statement: "
    );
    sqlite3_bind_int64(runStmt, 1, static_cast<sqlite3_int64>(run.spaceId));
    sqlite3_bind_int64(runStmt, 2, static_cast<sqlite3_int64>(run.deviceId));
    sqlite3_bind_int64(runStmt, 3, static_cast<sqlite3_int64>(run.deviceApiId));
    DatabaseUtility::BindOptionalText(runStmt, 4, run.inputData);

    const int result = sqlite3_step(runStmt);

    if (result != SQLITE_DONE)
    {
        std::string error = sqlite3_errmsg(connection);
        sqlite3_finalize(runStmt);
        throw KttException("Failed to execute run INSERT statement: " + error);
    }

    sqlite3_finalize(runStmt);
    return sqlite3_last_insert_rowid(connection);
}

} // namespace ktt::db
