#include "ResultRepository.h"

#include <json.hpp>
#include <sqlite3.h>

#include <Api/KttException.h>
#include <Output/JsonConverters.h>
#include <Repository/Result/ResultRepository.h>
#include <Repository/Utility.h>

namespace ktt::db
{

void ResultRepository::CreateResults(
    sqlite3 *connection, const size_t runId, const std::vector<KernelResult> &results, const int indentResultsJson
)
{
    const char *resultSql = R"(
        INSERT INTO tuning_result (run_id, duration, result)
        VALUES (?, ?, ?)
    )";

    sqlite3_stmt *resultStmt = DatabaseUtility::PrepareStatement(
        connection,
        resultSql,
        "Failed to prepare result INSERT statement: "
    );

    for (const auto &result : results)
    {
        if (result.GetStatus() != ResultStatus::Ok)
            continue;
        const auto jsonResult = json(result).dump(indentResultsJson);

        sqlite3_bind_int64(resultStmt, 1, static_cast<sqlite3_int64>(runId));
        sqlite3_bind_int64(resultStmt, 2, static_cast<sqlite3_int64>(result.GetKernelDuration()));
        sqlite3_bind_text(resultStmt, 3, jsonResult.c_str(), -1, SQLITE_TRANSIENT);

        const int executeResult = sqlite3_step(resultStmt);

        if (executeResult != SQLITE_DONE)
        {
            std::string error = sqlite3_errmsg(connection);
            sqlite3_finalize(resultStmt);
            throw KttException("Failed to execute result INSERT statement: " + error);
        }

        sqlite3_reset(resultStmt);
        sqlite3_clear_bindings(resultStmt);
    }

    sqlite3_finalize(resultStmt);
}

std::vector<KernelResult> ResultRepository::SimpleResultQuery(
    sqlite3 *connection, size_t spaceId, const Device &device, uint32_t limit
)
{
    const char *resultSql = R"(
        SELECT tuning_result.result
        FROM tuning_result
        JOIN tuning_run ON tuning_run.id = tuning_result.run_id
        JOIN device_info ON device_info.id = tuning_run.device_id
        JOIN device_api ON device_api.id = tuning_run.device_api_id
        WHERE tuning_run.space_id = ?
            AND ((device_info.name = ?
                    AND device_info.vendor = ?
                    AND device_info.type = ?)
                OR (device_api.compute_api_id = ?
                    AND device_api.version_major IS ?
                    AND (device_api.version_minor IS ? OR device_api.version_minor BETWEEN ? AND ?)
                    AND device_api.extensions IS ?))
        ORDER BY tuning_result.duration ASC
        LIMIT ?
    )";

    std::optional<int> minorLow;
    std::optional<int> minorHigh;
    if (device.computeApi == ComputeApi::CUDA && device.cudaComputeCapabilityMinor)
    {
        minorLow = *device.cudaComputeCapabilityMinor - 1;
        minorHigh = *device.cudaComputeCapabilityMinor + 1;
    }

    sqlite3_stmt *resultStmt = DatabaseUtility::PrepareStatement(
        connection,
        resultSql,
        "Failed to prepare tuning_result SELECT statement: "
    );

    sqlite3_bind_int64(resultStmt, 1, spaceId);
    sqlite3_bind_text(resultStmt, 2, device.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(resultStmt, 3, device.vendor.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(resultStmt, 4, device.type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(resultStmt, 5, (int) device.computeApi);
    DatabaseUtility::BindOptionalInt(resultStmt, 6, device.cudaComputeCapabilityMajor);
    DatabaseUtility::BindOptionalInt(resultStmt, 7, device.cudaComputeCapabilityMinor);
    DatabaseUtility::BindOptionalInt(resultStmt, 8, minorLow);
    DatabaseUtility::BindOptionalInt(resultStmt, 9, minorHigh);
    DatabaseUtility::BindOptionalText(resultStmt, 10, device.extensions);
    sqlite3_bind_int(resultStmt, 11, limit);

    std::vector<KernelResult> results;
    while (true)
    {
        const int result = sqlite3_step(resultStmt);

        if (result == SQLITE_DONE)
            break;

        if (result != SQLITE_ROW)
        {
            std::string error = sqlite3_errmsg(connection);
            sqlite3_finalize(resultStmt);
            throw KttException("Failed to execute tuning_result SELECT statement: " + error);
        }

        const auto jsonText = DatabaseUtility::ReadTextColumn(resultStmt, 0);
        const auto jsonResult = nlohmann::json::parse(jsonText);
        results.push_back(jsonResult.get<KernelResult>());
    }

    sqlite3_finalize(resultStmt);
    return results;
}

std::vector<KernelResult> ResultRepository::ResultsForRunIds(
    sqlite3 *connection, const std::vector<size_t> &runIds, uint32_t limit
)
{
    if (runIds.empty() || limit == 0)
        return {};

    std::string resultSql = "SELECT tuning_result.result FROM tuning_result WHERE tuning_result.run_id IN "
        + DatabaseUtility::SqlList(runIds.size())
        + " ORDER BY tuning_result.duration ASC LIMIT ?";

    sqlite3_stmt *resultStmt = DatabaseUtility::PrepareStatement(
        connection,
        resultSql.c_str(),
        "Failed to prepare tuning_result SELECT statement: "
    );

    int bindIndex = 1;
    for (const auto runId : runIds)
    {
        sqlite3_bind_int64(resultStmt, bindIndex, static_cast<sqlite3_int64>(runId));
        ++bindIndex;
    }
    sqlite3_bind_int(resultStmt, bindIndex, static_cast<int>(limit));

    std::vector<KernelResult> results;
    while (true)
    {
        const int result = sqlite3_step(resultStmt);

        if (result == SQLITE_DONE)
            break;

        if (result != SQLITE_ROW)
        {
            std::string error = sqlite3_errmsg(connection);
            sqlite3_finalize(resultStmt);
            throw KttException("Failed to execute tuning_result SELECT statement: " + error);
        }

        const auto jsonText = DatabaseUtility::ReadTextColumn(resultStmt, 0);
        const auto jsonResult = nlohmann::json::parse(jsonText);
        results.push_back(jsonResult.get<KernelResult>());
    }

    sqlite3_finalize(resultStmt);
    return results;
}

} // namespace ktt::db
