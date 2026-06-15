#include <sqlite3.h>

#include <Api/KttException.h>
#include <Repository/Run/RunRepository.h>
#include <Repository/Utility.h>

namespace ktt::db
{

size_t RunRepository::CreateRun(sqlite3 *connection, const Run &run)
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

std::vector<RunQueryResult> RunRepository::GetRunsForSpacePaged(
    sqlite3 *connection, size_t spaceId, size_t offset, size_t limit
)
{
    const char *runSql = R"(
        SELECT
            tuning_run.id,
            tuning_run.input_data,
            device_info.name,
            device_info.vendor,
            device_info.type,
            device_api.compute_api_id,
            device_api.version_major,
            device_api.version_minor,
            device_api.extensions
        FROM tuning_run
        JOIN device_info ON device_info.id = tuning_run.device_id
        JOIN device_api ON device_api.id = tuning_run.device_api_id
        WHERE tuning_run.space_id = ?
        ORDER BY tuning_run.id ASC
        LIMIT ? OFFSET ?
    )";

    sqlite3_stmt *runStmt = DatabaseUtility::PrepareStatement(
        connection,
        runSql,
        "Failed to prepare tuning_run SELECT statement: "
    );
    sqlite3_bind_int64(runStmt, 1, static_cast<sqlite3_int64>(spaceId));
    const sqlite3_int64 limitValue = limit == 0 ? static_cast<sqlite3_int64>(-1) : static_cast<sqlite3_int64>(limit);
    const sqlite3_int64 offsetValue = limit == 0 ? 0 : static_cast<sqlite3_int64>(offset);
    sqlite3_bind_int64(runStmt, 2, limitValue);
    sqlite3_bind_int64(runStmt, 3, offsetValue);

    std::vector<RunQueryResult> runs;

    while (true)
    {
        const int result = sqlite3_step(runStmt);

        if (result == SQLITE_DONE)
            break;

        if (result != SQLITE_ROW)
        {
            const std::string error = sqlite3_errmsg(connection);
            sqlite3_finalize(runStmt);
            throw KttException("Failed to execute tuning_run SELECT statement: " + error);
        }

        RunQueryResult row{};
        row.runId = static_cast<size_t>(sqlite3_column_int64(runStmt, 0));
        const bool inputIsNull = sqlite3_column_type(runStmt, 1) == SQLITE_NULL;
        row.inputData = inputIsNull ? std::nullopt
                                    : std::optional<std::string>(DatabaseUtility::ReadTextColumn(runStmt, 1));

        row.deviceInfo.name = DatabaseUtility::ReadTextColumn(runStmt, 2);
        row.deviceInfo.vendor = DatabaseUtility::ReadTextColumn(runStmt, 3);
        row.deviceInfo.type = DatabaseUtility::ReadTextColumn(runStmt, 4);
        row.deviceInfo.computeApi = static_cast<ComputeApi>(sqlite3_column_int(runStmt, 5));

        const bool majorIsNull = sqlite3_column_type(runStmt, 6) == SQLITE_NULL;
        const bool minorIsNull = sqlite3_column_type(runStmt, 7) == SQLITE_NULL;
        const bool extensionsIsNull = sqlite3_column_type(runStmt, 8) == SQLITE_NULL;

        if (!majorIsNull)
            row.deviceInfo.cudaComputeCapabilityMajor = static_cast<uint32_t>(sqlite3_column_int(runStmt, 6));
        if (!minorIsNull)
            row.deviceInfo.cudaComputeCapabilityMinor = static_cast<uint32_t>(sqlite3_column_int(runStmt, 7));
        if (!extensionsIsNull)
            row.deviceInfo.extensions = DatabaseUtility::ReadTextColumn(runStmt, 8);

        runs.push_back(std::move(row));
    }

    sqlite3_finalize(runStmt);
    return runs;
}

} // namespace ktt::db
