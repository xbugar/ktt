#include <sqlite3.h>
#include <string>

#include <Api/KttException.h>
#include <Database/Repository/Device/DeviceRepository.h>
#include <Database/Repository/Result/ResultRepository.h>
#include <Database/Repository/Run/RunRepository.h>
#include <Database/Repository/Source/SourceRepository.h>
#include <Database/Repository/Space/SpaceRepository.h>
#include <Database/Sync/DatabaseSync.h>
#include <Utility/Logger/Logger.h>

namespace ktt::db
{

namespace
{

void Execute(sqlite3 *connection, const char *sql, const std::string &errorPrefix)
{
    char *errorMsg = nullptr;
    if (sqlite3_exec(connection, sql, nullptr, nullptr, &errorMsg) != SQLITE_OK)
    {
        const std::string error = errorMsg ? errorMsg : "unknown error";
        sqlite3_free(errorMsg);
        throw KttException(errorPrefix + error);
    }
}

/** Recreates a single run (and its parent source/space/device + results) from the source in the target. */
void CopyRun(sqlite3 *target, sqlite3 *source, const RunSyncRecord &record)
{
    const auto sourceRow = SourceRepository::GetOrCreateSource(target, {std::nullopt, record.sourceFingerprint});

    const auto space = SpaceRepository::GetOrCreateSpace(
        target,
        {std::nullopt, *sourceRow.id, record.parameterFingerprint, record.spaceFingerprint}
    );

    const auto device = DeviceRepository::GetOrCreateDevice(
        target,
        {std::nullopt, // device Id
         std::nullopt, // api Id
         record.deviceInfo.name,
         record.deviceInfo.vendor,
         record.deviceInfo.type,
         record.deviceInfo.computeApi,
         record.deviceInfo.extensions,
         record.deviceInfo.cudaComputeCapabilityMajor,
         record.deviceInfo.cudaComputeCapabilityMinor}
    );

    const size_t newRunId = RunRepository::CreateRunWithGuid(
        target,
        {std::nullopt, // run Id
         *space.id,
         *device.id,
         *device.apiId,
         record.outputFormat,
         record.inputData},
        record.guid,
        record.createdAt
    );

    const auto rawResults = ResultRepository::GetRawResultsForRun(source, record.runId);
    ResultRepository::InsertRawResults(target, newRunId, rawResults);
}

} // namespace

size_t DatabaseSync::SyncRuns(sqlite3 *target, sqlite3 *source)
{
    const auto records = RunRepository::GetAllRuns(source);

    Execute(target, "BEGIN TRANSACTION;", "Failed to begin sync transaction: ");

    size_t inserted = 0;
    try
    {
        for (const auto &record : records)
        {
            if (RunRepository::RunExists(target, record.guid))
                continue;

            CopyRun(target, source, record);
            ++inserted;
        }
    }
    catch (...)
    {
        sqlite3_exec(target, "ROLLBACK;", nullptr, nullptr, nullptr);
        throw;
    }

    Execute(target, "COMMIT;", "Failed to commit sync transaction: ");

    ktt::Logger::LogInfo(
        "Database sync: " + std::to_string(inserted) + " new run(s) copied, "
        + std::to_string(records.size() - inserted) + " already present"
    );

    return inserted;
}

} // namespace ktt::db
