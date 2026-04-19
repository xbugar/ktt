#include <filesystem>
#include <iostream>
#include <sqlite3.h>
#include <utility>

#include <Api/KttException.h>
#include <Database/Database.h>
#include <Database/Repository/Device/DeviceRepository.h>
#include <Database/Repository/Result/ResultRepository.h>
#include <Database/Repository/Run/RunRepository.h>
#include <Database/Repository/Source/SourceRepository.h>
#include <Database/Repository/Space/SpaceRepository.h>
#include <Database/Schema/Schema.h>
#include <Database/Schema/Udts/TuningSourceUdt.h>
#include <Utility/Logger/Logger.h>

namespace ktt
{

Database::Database() : m_Connection(nullptr)
{
    m_DatabasePath = std::filesystem::path(std::getenv("HOME")) / ".local/share/ktt";
}

Database::Database(std::filesystem::path databasePath) : m_DatabasePath(std::move(databasePath)), m_Connection(nullptr)
{
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

void Database::SaveResultsForSource(const SaveTuningsDto &source) const
{
    OpenOrCreateDatabase();

    auto sourceUdt = SourceRepository::SelectSourceByFingerprint(
        m_Connection,
        source.sourceFingerprint,
        source.parameterFingerprint
    );

    size_t sourceId = 0;

    if (sourceUdt)
    {
        sourceId = sourceUdt->id;
        Logger::LogInfo("Using existing tuning source with id " + std::to_string(sourceId));
    } else
    {
        sourceId = SourceRepository::CreateSource(
            m_Connection,
            {source.parameterFingerprint, source.sourceFingerprint}
        );
        Logger::LogInfo("Created new tuning source with id " + std::to_string(sourceId));
    }

    auto spaceUdt = SpaceRepository::SelectSpaceByFingerprint(m_Connection, sourceId, source.spaceFingerprint);
    size_t spaceId = 0;

    if (spaceUdt)
    {
        spaceId = spaceUdt->id;
        Logger::LogInfo("Using existing tuning space with id " + std::to_string(spaceId));
    } else
    {
        spaceId = SpaceRepository::CreateSpace(m_Connection, {sourceId, source.spaceFingerprint});
        Logger::LogInfo("Created new tuning space with id " + std::to_string(spaceId));
    }

    const size_t deviceId = DeviceRepository::GetOrCreateDevice(
        m_Connection,
        source.computeApi,
        {
            source.device.Name,
            source.device.Vendor,
            source.device.Type,
            source.device.Extensions,
            source.device.cudaComputeCapabilityMajor,
            source.device.cudaComputeCapabilityMinor
        }
    );

    const size_t runId = RunRepository::CreateRun(m_Connection, spaceId, deviceId);
    ResultRepository::CreateResults(m_Connection, runId, source.results);
}

std::vector<KernelResult> Database::LoadBestResultsForSource(const LoadTuningsDto &source) const
{
    OpenOrCreateDatabase();

    const auto sourceUdt = SourceRepository::SelectSourceByFingerprint(
        m_Connection,
        source.sourceFingerprint,
        source.parameterFingerprint
    );

    if (!sourceUdt)
    {
        Logger::LogInfo("No results found for this source");
        return {};
    }

    if (source.computeApi == ComputeApi::Cpp)
    {
        Logger::LogInfo("No database compatibility matching for C++ compute API");
        return {};
    }

    return ResultRepository::SelectCompatibleBestResultsForSourceId(
        m_Connection,
        sourceUdt->id,
        {
            source.computeApi,
            source.device.Extensions,
            source.device.cudaComputeCapabilityMajor,
            source.limit
        }
    );
}

} // namespace ktt
