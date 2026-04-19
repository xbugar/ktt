#include <sqlite3.h>
#include <utility>

#include <Api/KttException.h>
#include <Database/Repository/Result/ResultRepository.h>
#include <Database/Repository/Utility.h>
#include <Database/Schema/Mappers.h>
#include <Database/Schema/Udts/TuningSourceUdt.h>
#include <Utility/Logger/Logger.h>

namespace ktt
{

void ResultRepository::CreateResults(sqlite3* connection, const size_t runId, const std::vector<KernelResult>& results)
{
    const char* resultSql = R"(
        INSERT INTO tuning_result (run_id, duration, result)
        VALUES (?, ?, ?)
    )";

    sqlite3_stmt* resultStmt = DatabaseUtility::PrepareStatement(
        connection,
        resultSql,
        "Failed to prepare result INSERT statement: "
    );

    for (const auto& result : results)
    {
        const auto jsonResult = json(result).dump();

        sqlite3_bind_int64(resultStmt, 1, static_cast<sqlite3_int64>(runId));
        sqlite3_bind_int64(resultStmt, 2, static_cast<sqlite3_int64>(result.GetKernelDuration()));
        sqlite3_bind_text(resultStmt, 3, jsonResult.c_str(), -1, SQLITE_TRANSIENT);

        const int executeResult = sqlite3_step(resultStmt);

        if (executeResult != SQLITE_DONE)
        {
            std::string error = sqlite3_errmsg(connection);
            sqlite3_finalize(resultStmt);
            throw KttException("Failed to execute result INSERT statement: " + error, ExceptionReason::Database);
        }

        sqlite3_reset(resultStmt);
        sqlite3_clear_bindings(resultStmt);
    }

    sqlite3_finalize(resultStmt);
}

std::vector<KernelResult> ResultRepository::SelectCompatibleBestResultsForSourceId(
    sqlite3* connection,
    const size_t sourceId,
    const CompatibleResultQuery& query)
{
    const char* openClSql = R"(
        SELECT
            tr.id,
            tr.duration,
            tr.result
        FROM tuning_result tr
        INNER JOIN tuning_run trn ON trn.id = tr.run_id
        INNER JOIN tuning_space ts ON ts.id = trn.space_id
        INNER JOIN device d ON d.id = trn.architecture_id
        INNER JOIN device_open_cl docl ON docl.device_id = d.id
        WHERE ts.source_id = ?
          AND docl.extensions = ?
        ORDER BY tr.duration ASC, tr.id ASC
        LIMIT ?
    )";

    const char* cudaSql = R"(
        SELECT
            tr.id,
            tr.duration,
            tr.result
        FROM tuning_result tr
        INNER JOIN tuning_run trn ON trn.id = tr.run_id
        INNER JOIN tuning_space ts ON ts.id = trn.space_id
        INNER JOIN device d ON d.id = trn.architecture_id
        INNER JOIN device_cuda dc ON dc.device_id = d.id
        WHERE ts.source_id = ?
          AND dc.version_major = ?
        ORDER BY tr.duration ASC, tr.id ASC
        LIMIT ?
    )";

    const char* vulkanSql = R"(
        SELECT
            tr.id,
            tr.duration,
            tr.result
        FROM tuning_result tr
        INNER JOIN tuning_run trn ON trn.id = tr.run_id
        INNER JOIN tuning_space ts ON ts.id = trn.space_id
        INNER JOIN device d ON d.id = trn.architecture_id
        INNER JOIN device_vulkan dv ON dv.device_id = d.id
        WHERE ts.source_id = ?
          AND dv.extensions = ?
        ORDER BY tr.duration ASC, tr.id ASC
        LIMIT ?
    )";

    auto executeQuery = [&](const char* sql, const auto& bindApiSpecificValue) -> std::vector<KernelResult>
    {
        sqlite3_stmt* statement = DatabaseUtility::PrepareStatement(
            connection,
            sql,
            "Failed to prepare compatible results SELECT statement: "
        );

        sqlite3_bind_int64(statement, 1, static_cast<sqlite3_int64>(sourceId));
        bindApiSpecificValue(statement);
        sqlite3_bind_int64(statement, 3, static_cast<sqlite3_int64>(query.limit));

        std::vector<KernelResult> output;
        int result = SQLITE_OK;

        while ((result = sqlite3_step(statement)) == SQLITE_ROW)
        {
            const auto* resultJson = reinterpret_cast<const char*>(sqlite3_column_text(statement, 2));

            if (resultJson == nullptr)
            {
                Logger::LogWarning("Missing tuning result JSON in database row. Skipping row.");
                continue;
            }

            try
            {
                output.push_back(json::parse(resultJson).get<KernelResult>());
            }
            catch (const std::exception& exception)
            {
                Logger::LogWarning(
                    "Failed to deserialize tuning result JSON: " + std::string(exception.what()) + ". Skipping row."
                );
            }
        }

        if (result != SQLITE_DONE)
        {
            const std::string error = sqlite3_errmsg(connection);
            sqlite3_finalize(statement);
            throw KttException("Failed to execute compatible results SELECT statement: " + error, ExceptionReason::Database);
        }

        sqlite3_finalize(statement);
        return output;
    };

    switch (query.computeApi)
    {
        case ComputeApi::OpenCL:
            return executeQuery(openClSql, [&](sqlite3_stmt* statement)
            {
                sqlite3_bind_text(statement, 2, query.deviceExtensions.c_str(), -1, SQLITE_TRANSIENT);
            });
        case ComputeApi::CUDA:
            return executeQuery(cudaSql, [&](sqlite3_stmt* statement)
            {
                sqlite3_bind_int(statement, 2, query.cudaComputeCapabilityMajor);
            });
        case ComputeApi::Vulkan:
            return executeQuery(vulkanSql, [&](sqlite3_stmt* statement)
            {
                sqlite3_bind_text(statement, 2, query.deviceExtensions.c_str(), -1, SQLITE_TRANSIENT);
            });
        case ComputeApi::Cpp:
            return {};
    }

    return {};
}

} // namespace ktt
