#include <Database.h>
#include <filesystem>
#include <sqlite3.h>
#include <utility>

#include <Api/Info/DatabaseTuningInfo.h>
#include <Repository/Device/DeviceRepository.h>
#include <Repository/Result/ResultRepository.h>
#include <Repository/Run/RunRepository.h>
#include <Repository/Source/SourceRepository.h>
#include <Repository/Space/SpaceRepository.h>
#include <Schema/Schema.h>
#include <Utility/Logger/Logger.h>

namespace ktt::db
{

Database::Database() : m_Connection(nullptr)
{
    m_DatabasePath = std::filesystem::path(std::getenv("HOME")) / ".local/share/ktt";
    ktt::Logger::LogInfo("Initializing database at " + m_DatabasePath.string());
    OpenOrCreateDatabase();
}

Database::Database(std::filesystem::path databasePath) : m_DatabasePath(std::move(databasePath)), m_Connection(nullptr)
{
    ktt::Logger::LogInfo("Initializing database at " + m_DatabasePath.string());
    OpenOrCreateDatabase();
}

Database::~Database()
{
    CloseDatabase();
}

void Database::OpenOrCreateDatabase() const
{
    if (m_Connection != nullptr)
        return;

    std::filesystem::create_directories(m_DatabasePath);

    const auto filePath = m_DatabasePath / "ktt.db";
    const int db = sqlite3_open(filePath.string().c_str(), &m_Connection);

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
    if (m_Connection != nullptr)
    {
        sqlite3_close(m_Connection);
        m_Connection = nullptr;
    }
}

void Database::SaveResultsForSource(const DatabaseTuningInfo &tuningInfo, std::vector<KernelResult> results) const
{
    Source source{std::nullopt, tuningInfo.sourceFingerprint};
    source = SourceRepository::GetOrCreateSource(m_Connection, source);

    Space space{std::nullopt, *source.id, tuningInfo.parameterFingerprint, tuningInfo.spaceFingerprint};
    space = SpaceRepository::GetOrCreateSpace(m_Connection, space);

    const auto device = DeviceRepository::GetOrCreateDevice(
        m_Connection,
        {
            std::nullopt,
            std::nullopt,
            tuningInfo.device.Name,
            tuningInfo.device.Vendor,
            tuningInfo.device.Type,
            tuningInfo.computeApi,
            tuningInfo.device.Extensions,
            tuningInfo.device.cudaComputeCapabilityMajor,
            tuningInfo.device.cudaComputeCapabilityMinor
        }
    );

    const size_t runId = RunRepository::CreateRun(
        m_Connection, 
        {
            std::nullopt,
            *space.id,
            *device.id,
            *device.apiId,
            tuningInfo.inputData
        }
    );
    ResultRepository::CreateResults(m_Connection, runId, results);
}

std::vector<KernelResult> Database::LoadBestResultsForSource(
    const DatabaseTuningInfo &tuningInfo,
    int limit
) const
{
    const auto source = SourceRepository::GetSourceByFingerprint(
        m_Connection,
        tuningInfo.sourceFingerprint
    );
    if (source == std::nullopt)
    {
        ktt::Logger::LogInfo("No results found for this source");
        return {};
    }

    const auto space = SpaceRepository::GetSpace(
        m_Connection,
        {
            std::nullopt,
            source.value().id.value(),
            tuningInfo.parameterFingerprint,
            tuningInfo.spaceFingerprint
        }
    );
    if (space == std::nullopt)
    {
        ktt::Logger::LogInfo("No results found for this source and parameter combination");
        return {};
    }

    return ResultRepository::SimpleResultQuery(
        m_Connection,
        space.value().id.value(),
        {   
            std::nullopt,
            std::nullopt,
            tuningInfo.device.Name,
            tuningInfo.device.Vendor,
            tuningInfo.device.Type,
            tuningInfo.computeApi,
            tuningInfo.device.Extensions,
            tuningInfo.device.cudaComputeCapabilityMajor,
            tuningInfo.device.cudaComputeCapabilityMinor
        },
        limit
    );
}

std::vector<KernelResult> Database::LoadBestResultsForSourceAndDevice(const GetResultsQuery &query) const
{
    return std::vector<KernelResult>();
}

} // namespace ktt::db
