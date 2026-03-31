#include <algorithm>
#include <sqlite3.h>
#include <utility>

#include <Api/KttException.h>
#include <Database/Record.h>
#include <Database/Repository/StatementRepository.h>
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

void BindRecordLookup(sqlite3_stmt* statement, const Record& data)
{
    sqlite3_bind_int64(statement, 1, static_cast<sqlite3_int64>(data.m_ParameterFingerprint));
    sqlite3_bind_int64(statement, 2, static_cast<sqlite3_int64>(data.m_SourceFingerprint));
    sqlite3_bind_int64(statement, 3, static_cast<sqlite3_int64>(data.m_TuningSpaceFingerprint));
    sqlite3_bind_text(statement, 4, data.m_Gpu.c_str(), -1, SQLITE_STATIC);
}

} // namespace

void StatementRepository::UpdateTuningRecord(sqlite3* connection, const Record& record)
{
    const char* updateSQL = R"(UPDATE tuning_records
                                SET best_result = ?, created_at = CURRENT_TIMESTAMP
                                WHERE parameter_fingerprint = ? AND source_fingerprint = ? AND tuning_space_fingerprint = ? AND gpu_architecture = ?)";

    sqlite3_stmt* statement = PrepareStatement(connection, updateSQL, "Failed to prepare UPDATE statement: ");

    const std::string jsonString = record.m_BestResult.dump();
    sqlite3_bind_text(statement, 1, jsonString.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(statement, 2, static_cast<sqlite3_int64>(record.m_ParameterFingerprint));
    sqlite3_bind_int64(statement, 3, static_cast<sqlite3_int64>(record.m_SourceFingerprint));
    sqlite3_bind_int64(statement, 4, static_cast<sqlite3_int64>(record.m_TuningSpaceFingerprint));
    sqlite3_bind_text(statement, 5, record.m_Gpu.c_str(), -1, SQLITE_STATIC);

    const int result = sqlite3_step(statement);
    if (result != SQLITE_DONE)
    {
        std::string error = sqlite3_errmsg(connection);
        sqlite3_finalize(statement);
        throw KttException("Failed to update record: " + error, ExceptionReason::Database);
    }

    sqlite3_finalize(statement);
}

void StatementRepository::InsertTuningRecord(sqlite3* connection, const Record& record)
{
    const char* insertSQL = R"(INSERT INTO tuning_records
                                (parameter_fingerprint, source_fingerprint, tuning_space_fingerprint, gpu_architecture, best_result)
                                VALUES (?, ?, ?, ?, ?))";

    sqlite3_stmt* statement = PrepareStatement(connection, insertSQL, "Failed to prepare INSERT statement: ");

    sqlite3_bind_int64(statement, 1, static_cast<sqlite3_int64>(record.m_ParameterFingerprint));
    sqlite3_bind_int64(statement, 2, static_cast<sqlite3_int64>(record.m_SourceFingerprint));
    sqlite3_bind_int64(statement, 3, static_cast<sqlite3_int64>(record.m_TuningSpaceFingerprint));
    sqlite3_bind_text(statement, 4, record.m_Gpu.c_str(), -1, SQLITE_STATIC);

    const std::string jsonString = record.m_BestResult.dump();
    sqlite3_bind_text(statement, 5, jsonString.c_str(), -1, SQLITE_TRANSIENT);

    const int result = sqlite3_step(statement);
    if (result != SQLITE_DONE)
    {
        std::string error = sqlite3_errmsg(connection);
        sqlite3_finalize(statement);
        throw KttException("Failed to insert record: " + error, ExceptionReason::Database);
    }

    sqlite3_finalize(statement);
}

std::unique_ptr<Record> StatementRepository::SelectTuningRecord(sqlite3* connection, const Record& data)
{
    const char* selectSQL =
        R"(SELECT parameter_fingerprint, source_fingerprint, tuning_space_fingerprint, gpu_architecture, best_result
                                FROM tuning_records
                                WHERE parameter_fingerprint = ? AND source_fingerprint = ? AND tuning_space_fingerprint = ? AND gpu_architecture = ?
                                ORDER BY created_at DESC
                                LIMIT 1)";

    sqlite3_stmt* statement = PrepareStatement(connection, selectSQL, "Failed to prepare SELECT statement: ");
    BindRecordLookup(statement, data);

    const int result = sqlite3_step(statement);

    if (result == SQLITE_DONE)
    {
        sqlite3_finalize(statement);
        return nullptr;
    }

    if (result != SQLITE_ROW)
    {
        std::string error = sqlite3_errmsg(connection);
        sqlite3_finalize(statement);
        throw KttException("Failed to execute SELECT statement: " + error, ExceptionReason::Database);
    }

    auto record = std::make_unique<Record>();
    record->m_ParameterFingerprint = static_cast<std::size_t>(sqlite3_column_int64(statement, 0));
    record->m_SourceFingerprint = static_cast<std::size_t>(sqlite3_column_int64(statement, 1));
    record->m_TuningSpaceFingerprint = static_cast<std::size_t>(sqlite3_column_int64(statement, 2));
    record->m_Gpu = reinterpret_cast<const char*>(sqlite3_column_text(statement, 3));

    const char* jsonText = reinterpret_cast<const char*>(sqlite3_column_text(statement, 4));
    if (jsonText != nullptr)
    {
        try
        {
            record->m_BestResult = json::parse(jsonText);
        }
        catch (const json::parse_error& e)
        {
            sqlite3_finalize(statement);
            Logger::LogWarning("Failed to parse JSON from database: " + std::string(e.what()) + ". Returning nullptr.");
            return nullptr;
        }
    }

    sqlite3_finalize(statement);
    return record;
}

std::unique_ptr<TuningSourceLoadUdt> StatementRepository::SelectSourceByFingerprints(sqlite3* connection,
    const TuningSourceSaveUdt& source)
{
    const char* sourceSQL = R"(
        SELECT id, parameter_fingerprint, source_fingerprint, created_at
        FROM tuning_source
        WHERE parameter_fingerprint = ? AND source_fingerprint = ?
        LIMIT 1
    )";

    sqlite3_stmt* sourceStmt = PrepareStatement(connection, sourceSQL, "Failed to prepare source SELECT statement: ");
    sqlite3_bind_int64(sourceStmt, 1, static_cast<sqlite3_int64>(source.parameterFingerprint));
    sqlite3_bind_int64(sourceStmt, 2, static_cast<sqlite3_int64>(source.sourceFingerprint));

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

    auto output = std::make_unique<TuningSourceLoadUdt>(Mappers::MapSourceLoadRow(sourceStmt));
    sqlite3_finalize(sourceStmt);

    return output;
}

void StatementRepository::SelectTopResultsForSourceId(sqlite3* connection, const std::size_t sourceId,
    TuningSourceLoadUdt& source, const std::size_t limit)
{

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
            rr.id,
            rr.run_id,
            rr.space_id,
            rr.duration,
            rr.result
        FROM ranked_results rr
        INNER JOIN tuning_space ts ON ts.id = rr.space_id
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
        const std::size_t spaceId = static_cast<std::size_t>(sqlite3_column_int64(resultsStmt, 0));

        auto spaceIterator = std::find_if(source.spaces.begin(), source.spaces.end(),
            [spaceId](const TuningSpaceLoadUdt& space)
            {
                return space.id == spaceId;
            });

        if (spaceIterator == source.spaces.end())
        {
            source.spaces.push_back(Mappers::MapSpaceLoadRow(resultsStmt));
            spaceIterator = std::prev(source.spaces.end());
        }

        std::string parseError;
        auto mappedResult = Mappers::MapResultLoadRow(resultsStmt, 4, &parseError);
        if (!mappedResult.has_value())
        {
            Logger::LogWarning("Failed to parse tuning result JSON: " + parseError + ". Skipping row.");
            continue;
        }

        spaceIterator->results.push_back(std::move(mappedResult.value()));
    }

    if (result != SQLITE_DONE)
    {
        std::string error = sqlite3_errmsg(connection);
        sqlite3_finalize(resultsStmt);
        throw KttException("Failed to execute best results SELECT statement: " + error, ExceptionReason::Database);
    }

    sqlite3_finalize(resultsStmt);
}

} // namespace ktt


