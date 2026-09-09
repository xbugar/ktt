#include <filesystem>
#include <sqlite3.h>
#include <utility>

#include <Api/Info/DatabaseTuningInfo.h>
#include <Api/KttException.h>
#include <Database/Database.h>
#include <Database/Repository/Device/DeviceRepository.h>
#include <Database/Repository/Result/ResultRepository.h>
#include <Database/Repository/Run/RunRepository.h>
#include <Database/Repository/Source/SourceRepository.h>
#include <Database/Repository/Space/SpaceRepository.h>
#include <Database/Schema/Schema.h>
#include <Database/Utility/TransactionGuard.h>
#include <Database/Utility/TuningInfoValidation.h>
#include <Output/OutputFormat.h>
#include <Utility/Logger/Logger.h>

namespace ktt::db
{

Database::Database() : m_Connection(nullptr)
{
    m_DatabasePath = std::filesystem::path(std::getenv("HOME")) / ".local/share/ktt";
    std::filesystem::create_directories(m_DatabasePath);
    m_DatabasePath /= "ktt.db";

    ktt::Logger::LogInfo("Initializing database at " + m_DatabasePath.string());
    OpenOrCreateDatabase();
}

Database::Database(std::filesystem::path databasePath) : m_DatabasePath(std::move(databasePath)), m_Connection(nullptr)
{
    ktt::Logger::LogInfo("Initializing database at " + m_DatabasePath.string());
    OpenOrCreateDatabase();
}

Database::Database(sqlite3 *connection) : m_Connection(connection), m_OwnsConnection(false)
{
    if (m_Connection == nullptr)
        throw KttException("Cannot construct Database from a null SQLite connection");

    const char *filename = sqlite3_db_filename(m_Connection, "main");
    m_DatabasePath = (filename != nullptr) ? std::filesystem::path(filename) : std::filesystem::path();

    ktt::Logger::LogInfo("Initializing database from existing SQLite connection at " + m_DatabasePath.string());
    sqlite3_exec(m_Connection, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);
    Schema::CreateIfNotExists(m_Connection);
}


Database::~Database()
{
    CloseDatabase();
}

void Database::OpenOrCreateDatabase() const
{
    if (m_Connection != nullptr)
        return;

    const int db = sqlite3_open(m_DatabasePath.string().c_str(), &m_Connection);

    if (db != SQLITE_OK)
    {
        std::string error = sqlite3_errmsg(m_Connection);
        sqlite3_close(m_Connection);
        m_Connection = nullptr;
        throw std::runtime_error("Failed to open or create database: " + error);
    }
    sqlite3_exec(m_Connection, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);
    Schema::CreateIfNotExists(m_Connection);
}

void Database::CloseDatabase() const
{
    if (m_OwnsConnection && m_Connection != nullptr)
    {
        sqlite3_close(m_Connection);
        m_Connection = nullptr;
    }
}

void Database::SaveResults(const TuningInfo &tuningInfo, std::vector<KernelResult> results, SaveOptions option) const
{
    ValidateTuningInfo(tuningInfo);

    try
    {
        TransactionGuard transaction(m_Connection);

        const auto source = SourceRepository::GetOrCreateSource(
            m_Connection,
            {std::nullopt, tuningInfo.spaceInfo.sourceFingerprint}
        );

        const auto space = SpaceRepository::GetOrCreateSpace(
            m_Connection,
            {std::nullopt, // space Id
             *source.id,
             tuningInfo.spaceInfo.parameterFingerprint,
             tuningInfo.spaceInfo.spaceFingerprint}
        );

        const auto device = DeviceRepository::GetOrCreateDevice(
            m_Connection,
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
            m_Connection,
            {std::nullopt, // run Id
             *space.id,
             *device.id,
             *device.apiId,
             option.format,
             tuningInfo.inputData,
             tuningInfo.device.deviceIdentifier}
        );

        ResultRepository::CreateResults(m_Connection, runId, results, option.format, option.indent);
        transaction.Commit();
    } catch (...)
    {
        throw;
    }
}

std::vector<KernelResult> Database::SimpleGetBestResults(const TuningInfo &t, uint32_t limit) const
{
    ValidateTuningInfo(t);

    const auto source = SourceRepository::GetSource(m_Connection, t.spaceInfo.sourceFingerprint);
    if (source == std::nullopt)
    {
        ktt::Logger::LogInfo("No results found for this source");
        return {};
    }

    const auto space = SpaceRepository::GetSpace(
        m_Connection,
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

    return ResultRepository::SimpleGetBestResults(
        m_Connection,
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

std::vector<KernelResult> Database::GetBestResults(const GetBestResul``tsQuery &query) const
{
    if (query.limit <= 0)
        return {};

    const auto source = SourceRepository::GetSource(m_Connection, query.source.sourceFingerprint);
    if (source == std::nullopt)
    {
        ktt::Logger::LogInfo("No results found for this source");
        return {};
    }

    const auto space = SpaceRepository::GetSpace(
        m_Connection,
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

    for (auto runs = RunRepository::GetRunsBySpaceId(m_Connection, space.value().id.value(), offset, RunBatchSize);
         !runs.empty();
         runs = RunRepository::GetRunsBySpaceId(m_Connection, space.value().id.value(), offset, RunBatchSize))
    {

        offset += runs.size();

        std::vector<size_t> runIds;
        runIds.reserve(runs.size());

        for (const auto &run : runs)
        {
            bool keep = true;
            if (query.devicePredicate)
                keep = keep && (*query.devicePredicate)(run.deviceInfo);
            if (query.inputPredicate)
            {
                if (run.inputData)
                    keep = keep && (*query.inputPredicate)(*run.inputData);
                else
                    keep = false;
            }

            if (keep)
                runIds.push_back(run.runId);
        }

        if (runIds.empty())
            continue;

        auto batchResults = ResultRepository::ResultsByRunIds(m_Connection, runIds, query.limit);

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
    return SourceRepository::GetStatsForSource(m_Connection, sourceFingerprint);
}

size_t Database::SyncFromFile(const std::filesystem::path &sourceDatabase) const
{
    if (!std::filesystem::exists(sourceDatabase))
        throw KttException("Cannot sync: database file does not exist: " + sourceDatabase.string());

    sqlite3 *source = nullptr;
    const int open = sqlite3_open_v2(sourceDatabase.string().c_str(), &source, SQLITE_OPEN_READONLY, nullptr);

    if (open != SQLITE_OK)
    {
        const std::string error = sqlite3_errmsg(source);
        sqlite3_close(source);
        throw KttException("Failed to open source database for sync: " + error);
    }

    ktt::Logger::LogInfo("Syncing runs from " + sourceDatabase.string() + " into " + m_DatabasePath.string());

    size_t inserted = 0;
    try
    {
        TransactionGuard transaction(m_Connection);

        const auto records = RunRepository::GetAllRuns(source);

        for (const auto &record : records)
        {
            if (RunRepository::RunExists(m_Connection, record.guid))
                continue;

            const auto sourceRow = SourceRepository::GetOrCreateSource(
                m_Connection,
                {std::nullopt, record.sourceFingerprint}
            );

            const auto space = SpaceRepository::GetOrCreateSpace(
                m_Connection,
                {std::nullopt, *sourceRow.id, record.parameterFingerprint, record.spaceFingerprint}
            );

            const auto device = DeviceRepository::GetOrCreateDevice(
                m_Connection,
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
                m_Connection,
                {std::nullopt, // run Id
                 *space.id,
                 *device.id,
                 *device.apiId,
                 record.outputFormat,
                 record.inputData,
                 record.deviceInfo.deviceIdentifier},
                record.guid,
                record.createdAt
            );

            const auto rawResults = ResultRepository::GetRawResultsByRunId(source, record.runId);
            ResultRepository::InsertRawResults(m_Connection, newRunId, rawResults);

            ++inserted;
        }

        transaction.Commit();

        ktt::Logger::LogInfo(
            "Database sync: " + std::to_string(inserted) + " new run(s) copied, " +
            std::to_string(records.size() - inserted) + " already present"
        );
    } catch (...)
    {
        sqlite3_close(source);
        throw;
    }

    sqlite3_close(source);
    return inserted;
}

} // namespace ktt::db
