#include <sqlite3.h>

#include <Api/KttException.h>
#include <Database/Repository/Run/RunRepository.h>
#include <Database/Repository/Utility.h>
#include <Database/Utility/Uuid.h>

namespace ktt::db
{

size_t RunRepository::CreateRun(sqlite3 *connection, const Run &run)
{
    const char *runSQL = R"(
        INSERT INTO tuning_run (guid, space_id, device_id, device_api_id, output_format_id, input_data)
        VALUES (?, ?, ?, ?, ?, ?)
    )";

    sqlite3_stmt *runStmt = DatabaseUtility::PrepareStatement(
        connection,
        runSQL,
        "Failed to prepare run INSERT statement: "
    );
    const uuid guid = UuidGenerator::GenerateUuid();
    DatabaseUtility::BindUuid(runStmt, 1, guid);
    sqlite3_bind_int64(runStmt, 2, static_cast<sqlite3_int64>(run.spaceId));
    sqlite3_bind_int64(runStmt, 3, static_cast<sqlite3_int64>(run.deviceId));
    sqlite3_bind_int64(runStmt, 4, static_cast<sqlite3_int64>(run.deviceApiId));
    sqlite3_bind_int(runStmt, 5, static_cast<int>(run.outputFormat));
    DatabaseUtility::BindOptionalText(runStmt, 6, run.inputData);

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

size_t RunRepository::CreateRunWithGuid(
    sqlite3 *connection, const Run &run, const uuid &guid, const std::string &createdAt
)
{
    const char *runSQL = R"(
        INSERT INTO tuning_run (guid, space_id, device_id, device_api_id, output_format_id, input_data, created_at)
        VALUES (?, ?, ?, ?, ?, ?, ?)
    )";

    sqlite3_stmt *runStmt = DatabaseUtility::PrepareStatement(
        connection,
        runSQL,
        "Failed to prepare run INSERT statement: "
    );
    DatabaseUtility::BindUuid(runStmt, 1, guid);
    sqlite3_bind_int64(runStmt, 2, static_cast<sqlite3_int64>(run.spaceId));
    sqlite3_bind_int64(runStmt, 3, static_cast<sqlite3_int64>(run.deviceId));
    sqlite3_bind_int64(runStmt, 4, static_cast<sqlite3_int64>(run.deviceApiId));
    sqlite3_bind_int(runStmt, 5, static_cast<int>(run.outputFormat));
    DatabaseUtility::BindOptionalText(runStmt, 6, run.inputData);
    sqlite3_bind_text(runStmt, 7, createdAt.c_str(), -1, SQLITE_TRANSIENT);

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

bool RunRepository::RunExists(sqlite3 *connection, const uuid &guid)
{
    const char *runSQL = R"(
        SELECT 1 FROM tuning_run WHERE guid = ? LIMIT 1
    )";

    sqlite3_stmt *runStmt = DatabaseUtility::PrepareStatement(
        connection,
        runSQL,
        "Failed to prepare run existence SELECT statement: "
    );
    DatabaseUtility::BindUuid(runStmt, 1, guid);

    const int result = sqlite3_step(runStmt);

    if (result != SQLITE_ROW && result != SQLITE_DONE)
    {
        const std::string error = sqlite3_errmsg(connection);
        sqlite3_finalize(runStmt);
        throw KttException("Failed to execute run existence SELECT statement: " + error);
    }

    sqlite3_finalize(runStmt);
    return result == SQLITE_ROW;
}

std::vector<RunSyncRecord> RunRepository::GetAllRuns(sqlite3 *connection)
{
    const char *runSql = R"(
        SELECT
            tuning_run.id,
            tuning_run.guid,
            tuning_source.source_fingerprint,
            tuning_space.parameter_fingerprint,
            tuning_space.space_fingerprint,
            device_info.name,
            device_info.vendor,
            device_info.type,
            device_api.compute_api_id,
            device_api.version_major,
            device_api.version_minor,
            device_api.extensions,
            tuning_run.output_format_id,
            tuning_run.input_data,
            tuning_run.created_at
        FROM tuning_run
        JOIN tuning_space ON tuning_space.id = tuning_run.space_id
        JOIN tuning_source ON tuning_source.id = tuning_space.source_id
        JOIN device_info ON device_info.id = tuning_run.device_id
        JOIN device_api ON device_api.id = tuning_run.device_api_id
        ORDER BY tuning_run.id ASC
    )";

    sqlite3_stmt *runStmt = DatabaseUtility::PrepareStatement(
        connection,
        runSql,
        "Failed to prepare tuning_run sync SELECT statement: "
    );

    std::vector<RunSyncRecord> runs;

    while (true)
    {
        const int result = sqlite3_step(runStmt);

        if (result == SQLITE_DONE)
            break;

        if (result != SQLITE_ROW)
        {
            const std::string error = sqlite3_errmsg(connection);
            sqlite3_finalize(runStmt);
            throw KttException("Failed to execute tuning_run sync SELECT statement: " + error);
        }

        RunSyncRecord row{};
        row.runId = static_cast<size_t>(sqlite3_column_int64(runStmt, 0));
        row.guid = DatabaseUtility::ReadUuidColumn(runStmt, 1);
        row.sourceFingerprint = static_cast<size_t>(std::stoull(DatabaseUtility::ReadTextColumn(runStmt, 2)));
        row.parameterFingerprint = static_cast<size_t>(std::stoull(DatabaseUtility::ReadTextColumn(runStmt, 3)));
        row.spaceFingerprint = static_cast<size_t>(std::stoull(DatabaseUtility::ReadTextColumn(runStmt, 4)));

        row.deviceInfo.name = DatabaseUtility::ReadTextColumn(runStmt, 5);
        row.deviceInfo.vendor = DatabaseUtility::ReadTextColumn(runStmt, 6);
        row.deviceInfo.type = DatabaseUtility::ReadTextColumn(runStmt, 7);
        row.deviceInfo.computeApi = static_cast<ComputeApi>(sqlite3_column_int(runStmt, 8));

        const bool majorIsNull = sqlite3_column_type(runStmt, 9) == SQLITE_NULL;
        const bool minorIsNull = sqlite3_column_type(runStmt, 10) == SQLITE_NULL;
        const bool extensionsIsNull = sqlite3_column_type(runStmt, 11) == SQLITE_NULL;

        if (!majorIsNull)
            row.deviceInfo.cudaComputeCapabilityMajor = static_cast<uint32_t>(sqlite3_column_int(runStmt, 9));
        if (!minorIsNull)
            row.deviceInfo.cudaComputeCapabilityMinor = static_cast<uint32_t>(sqlite3_column_int(runStmt, 10));
        if (!extensionsIsNull)
            row.deviceInfo.extensions = DatabaseUtility::ReadTextColumn(runStmt, 11);

        row.outputFormat = static_cast<ktt::OutputFormat>(sqlite3_column_int(runStmt, 12));

        const bool inputIsNull = sqlite3_column_type(runStmt, 13) == SQLITE_NULL;
        row.inputData = inputIsNull ? std::nullopt
                                    : std::optional<std::string>(DatabaseUtility::ReadTextColumn(runStmt, 13));

        row.createdAt = DatabaseUtility::ReadTextColumn(runStmt, 14);

        runs.push_back(std::move(row));
    }

    sqlite3_finalize(runStmt);
    return runs;
}

std::vector<RunQueryResult> RunRepository::GetRunsBySpaceId(
    sqlite3 *connection, size_t spaceId, size_t offset, size_t limit
)
{
    const char *runSql = R"(
        SELECT
            tuning_run.id,
            tuning_run.guid,
            tuning_run.input_data,
            device_info.name,
            device_info.vendor,
            device_info.type,
            device_api.compute_api_id,
            device_api.version_major,
            device_api.version_minor,
            device_api.extensions,
            tuning_run.output_format_id
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
        row.guid = DatabaseUtility::ReadUuidColumn(runStmt, 1);

        const bool inputIsNull = sqlite3_column_type(runStmt, 2) == SQLITE_NULL;
        row.inputData = inputIsNull ? std::nullopt
                                    : std::optional<std::string>(DatabaseUtility::ReadTextColumn(runStmt, 2));

        row.deviceInfo.name = DatabaseUtility::ReadTextColumn(runStmt, 3);
        row.deviceInfo.vendor = DatabaseUtility::ReadTextColumn(runStmt, 4);
        row.deviceInfo.type = DatabaseUtility::ReadTextColumn(runStmt, 5);
        row.deviceInfo.computeApi = static_cast<ComputeApi>(sqlite3_column_int(runStmt, 6));

        const bool majorIsNull = sqlite3_column_type(runStmt, 7) == SQLITE_NULL;
        const bool minorIsNull = sqlite3_column_type(runStmt, 8) == SQLITE_NULL;
        const bool extensionsIsNull = sqlite3_column_type(runStmt, 9) == SQLITE_NULL;

        if (!majorIsNull)
            row.deviceInfo.cudaComputeCapabilityMajor = static_cast<uint32_t>(sqlite3_column_int(runStmt, 7));
        if (!minorIsNull)
            row.deviceInfo.cudaComputeCapabilityMinor = static_cast<uint32_t>(sqlite3_column_int(runStmt, 8));
        if (!extensionsIsNull)
            row.deviceInfo.extensions = DatabaseUtility::ReadTextColumn(runStmt, 9);

        row.outputFormat = static_cast<ktt::OutputFormat>(sqlite3_column_int(runStmt, 10));

        runs.push_back(std::move(row));
    }

    sqlite3_finalize(runStmt);
    return runs;
}

} // namespace ktt::db
