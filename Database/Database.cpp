#include <Database.h>
#include <filesystem>
#include <sqlite3.h>
#include <utility>

#include <Api/Info/DatabaseTuningInfo.h>
#include <Output/OutputFormat.h>
#include <Repository/Device/DeviceRepository.h>
#include <Repository/Result/ResultRepository.h>
#include <Repository/Run/RunRepository.h>
#include <Repository/Source/SourceRepository.h>
#include <Repository/Space/SpaceRepository.h>
#include <Schema/Schema.h>
#include <Utility/Logger/Logger.h>

namespace ktt::db
{

Database::Database(const ktt::OutputFormat format, const int indentResultsJson) :
    Connection(nullptr), IndentResultsJson(indentResultsJson), OutputFormat(format)
{
    DatabasePath = std::filesystem::path(std::getenv("HOME")) / ".local/share/ktt";
    std::filesystem::create_directories(DatabasePath);
    DatabasePath /= "ktt.db";

    ktt::Logger::LogInfo("Initializing database at " + DatabasePath.string());
    OpenOrCreateDatabase();
}

Database::Database(const ktt::OutputFormat format, std::filesystem::path databasePath, const int indentResultsJson) :
    DatabasePath(std::move(databasePath)), Connection(nullptr), IndentResultsJson(indentResultsJson),
    OutputFormat(format)
{
    ktt::Logger::LogInfo("Initializing database at " + DatabasePath.string());
    OpenOrCreateDatabase();
}


Database::~Database()
{
    CloseDatabase();
}

void Database::OpenOrCreateDatabase() const
{
    if (Connection != nullptr)
        return;

    const int db = sqlite3_open(DatabasePath.string().c_str(), &Connection);

    if (db != SQLITE_OK)
    {
        std::string error = sqlite3_errmsg(Connection);
        sqlite3_close(Connection);
        Connection = nullptr;
        throw std::runtime_error("Failed to open or create database: " + error);
    }
    sqlite3_exec(Connection, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);
    Schema::CreateIfNotExists(Connection);
}

void Database::CloseDatabase() const
{
    if (Connection != nullptr)
    {
        sqlite3_close(Connection);
        Connection = nullptr;
    }
}

void Database::SaveResultsForSource(const TuningInfo &tuningInfo, std::vector<KernelResult> results) const
{
    const auto source = SourceRepository::GetOrCreateSource(
        Connection,
        {std::nullopt, tuningInfo.spaceInfo.sourceFingerprint}
    );

    const auto space = SpaceRepository::GetOrCreateSpace(
        Connection,
        {std::nullopt, // space Id
         *source.id,
         tuningInfo.spaceInfo.parameterFingerprint,
         tuningInfo.spaceInfo.spaceFingerprint}
    );

    const auto device = DeviceRepository::GetOrCreateDevice(
        Connection,
        {std::nullopt, // device Id
         std::nullopt, // api Id
         tuningInfo.device.name,
         tuningInfo.device.vendor,
         tuningInfo.device.type,
         tuningInfo.device.computeApi,
         tuningInfo.device.extensions,
         tuningInfo.device.cudaComputeCapabilityMajor,
         tuningInfo.device.cudaComputeCapabilityMinor}
    );

    const size_t runId = RunRepository::CreateRun(
        Connection,
        {std::nullopt, // run Id
         *space.id,
         *device.id,
         *device.apiId,
         tuningInfo.inputData}
    );

    ResultRepository::CreateResults(Connection, runId, results, IndentResultsJson);
}

std::vector<KernelResult> Database::SimpleGetBestResultsForSource(const TuningInfo &t, uint32_t limit) const
{
    const auto source = SourceRepository::GetSource(Connection, t.spaceInfo.sourceFingerprint);
    if (source == std::nullopt)
    {
        ktt::Logger::LogInfo("No results found for this source");
        return {};
    }

    const auto space = SpaceRepository::GetSpace(
        Connection,
        {std::nullopt, // space Id
         source.value().id.value(),
         t.spaceInfo.parameterFingerprint,
         t.spaceInfo.spaceFingerprint}
    );
    if (space == std::nullopt)
    {
        ktt::Logger::LogInfo("No results found for this source and parameter combination");
        return {};
    }

    return ResultRepository::SimpleResultQuery(
        Connection,
        space.value().id.value(),
        {std::nullopt, // device Id
         std::nullopt, // api Id
         t.device.name,
         t.device.vendor,
         t.device.type,
         t.device.computeApi,
         t.device.extensions,
         t.device.cudaComputeCapabilityMajor,
         t.device.cudaComputeCapabilityMinor},
        limit
    );
}

std::vector<KernelResult> Database::GetBestResults(const GetResultsQuery &query) const
{
    if (query.limit <= 0)
        return {};

    const auto source = SourceRepository::GetSource(Connection, query.source.sourceFingerprint);
    if (source == std::nullopt)
    {
        ktt::Logger::LogInfo("No results found for this source");
        return {};
    }

    const auto space = SpaceRepository::GetSpace(
        Connection,
        {std::nullopt, // space Id
         source.value().id.value(),
         query.source.parameterFingerprint,
         query.source.spaceFingerprint}
    );
    if (space == std::nullopt)
    {
        ktt::Logger::LogInfo("No results found for this source and parameter combination");
        return {};
    }

    std::vector<KernelResult> bestResults;
    size_t offset = 0;

    for (auto runs = RunRepository::GetRunsForSpacePaged(Connection, space.value().id.value(), offset, RunBatchSize);
         !runs.empty();
         runs = RunRepository::GetRunsForSpacePaged(Connection, space.value().id.value(), offset, RunBatchSize))
    {

        offset += runs.size();

        std::vector<size_t> runIds;
        runIds.reserve(runs.size());

        for (const auto &run : runs)
        {
            bool keep = true;
            if (query.devicePredicate)
                keep = keep && (*query.devicePredicate)(run.deviceInfo);
            if (query.stringPredicate)
            {
                if (run.inputData)
                    keep = keep && (*query.stringPredicate)(*run.inputData);
                else
                    keep = false;
            }

            if (keep)
                runIds.push_back(run.runId);
        }

        if (runIds.empty())
            continue;

        auto batchResults = ResultRepository::ResultsForRunIds(Connection, runIds, query.limit);

        if (batchResults.empty())
            continue;

        bestResults.insert(bestResults.end(), batchResults.begin(), batchResults.end());

        std::sort(bestResults.begin(), bestResults.end(), [](const KernelResult &left, const KernelResult &right) {
            return left.GetTotalDuration() < right.GetTotalDuration();
        });

        if (bestResults.size() > query.limit)
            bestResults.resize(query.limit);
    }

    return bestResults;
}

std::optional<SourceStats> Database::GetStatsForSource(const size_t sourceFingerprint) const
{
    return SourceRepository::GetStatsForSource(Connection, sourceFingerprint);
}

} // namespace ktt::db
