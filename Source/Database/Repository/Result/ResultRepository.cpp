#include <sqlite3.h>
#include <utility>

#include <Api/KttException.h>
#include <Database/Repository/Result/ResultRepository.h>
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

void ResultRepository::CreateResults(sqlite3* connection, const size_t runId, const size_t spaceId,
    const std::vector<KernelResult>& results)
{
    const char* resultSql = R"(
        INSERT INTO tuning_result (run_id, space_id, duration, result)
        VALUES (?, ?, ?, ?)
    )";

    sqlite3_stmt* resultStmt = PrepareStatement(connection, resultSql, "Failed to prepare result INSERT statement: ");

    for (const auto& result : results)
    {
        const auto jsonResult = json(result).dump();

        sqlite3_bind_int64(resultStmt, 1, static_cast<sqlite3_int64>(runId));
        sqlite3_bind_int64(resultStmt, 2, static_cast<sqlite3_int64>(spaceId));
        sqlite3_bind_int64(resultStmt, 3, static_cast<sqlite3_int64>(result.GetKernelDuration()));
        sqlite3_bind_text(resultStmt, 4, jsonResult.c_str(), -1, SQLITE_TRANSIENT);

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


std::unique_ptr<std::vector<TuningResultLoadUdt>>
ResultRepository::SelectTopResultsForSourceId(sqlite3 *connection, const std::size_t sourceId,
                                                 const std::size_t limit)
{
    auto topResults = std::make_unique<std::vector<TuningResultLoadUdt>>();

    const char* resultsSQL = R"(
        WITH ranked_results AS
        (
            SELECT
                tr.id,
                tr.run_id,
                tr.space_id,
                tr.duration,
                tr.result,
                ROW_NUMBER() OVER (PARTITION BY tr.run_id ORDER BY tr.duration ASC, tr.id ASC) AS rank_in_run
            FROM tuning_result tr
            INNER JOIN tuning_space ts ON ts.id = tr.space_id
            WHERE ts.source_id = ?
        )
        SELECT
            ts.id,
            ts.source_id,
            ts.space_fingerprint,
            ts.created_at,
            trn.id,
            trn.uuid,
            trn.created_at,
            rr.id,
            rr.duration,
            rr.result
        FROM ranked_results rr
        INNER JOIN tuning_space ts ON ts.id = rr.space_id
        INNER JOIN tuning_run trn ON trn.id = rr.run_id
        WHERE rr.rank_in_run = 1
        ORDER BY rr.duration ASC, rr.id ASC
        LIMIT ?
    )";

    sqlite3_stmt* resultsStmt = PrepareStatement(connection, resultsSQL, "Failed to prepare best results SELECT statement: ");
    sqlite3_bind_int64(resultsStmt, 1, static_cast<sqlite3_int64>(sourceId));
    sqlite3_bind_int64(resultsStmt, 2, static_cast<sqlite3_int64>(limit));

    int result = SQLITE_OK;

    while ((result = sqlite3_step(resultsStmt)) == SQLITE_ROW)
    {
        std::string parseError;
        auto mappedResult = Mappers::MapResultLoadRow(resultsStmt, 7, &parseError);
        if (!mappedResult.has_value())
        {
            Logger::LogWarning("Failed to parse tuning result JSON: " + parseError + ". Skipping row.");
            continue;
        }

        mappedResult->tuningSpace = Mappers::MapSpaceLoadRow(resultsStmt, 0);
        mappedResult->tuningRun = Mappers::MapRunLoadRow(resultsStmt, 4);
        topResults->push_back(std::move(mappedResult.value()));
    }

    if (result != SQLITE_DONE)
    {
        std::string error = sqlite3_errmsg(connection);
        sqlite3_finalize(resultsStmt);
        throw KttException("Failed to execute best results SELECT statement: " + error, ExceptionReason::Database);
    }

    sqlite3_finalize(resultsStmt);
    return topResults;
}

} // namespace ktt


