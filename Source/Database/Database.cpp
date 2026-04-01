#include <filesystem>
#include <iostream>
#include <sqlite3.h>
#include <utility>

#include <Api/KttException.h>
#include <Database/Database.h>
#include <Database/Schema/Schema.h>
#include <Database/Schema/Udts/TuningSourceUdt.h>
#include <Utility/Logger/Logger.h>

#include <Database/Repository/Result/ResultRepository.h>
#include <Database/Repository/Run/RunRepository.h>
#include <Database/Repository/Space/SpaceRepository.h>
#include <Database/Repository/Source/SourceRepository.h>

namespace ktt
{

Database::Database() :
    m_Connection(nullptr)
{
    m_DatabasePath = std::filesystem::path(std::getenv("HOME")) / ".local/share/ktt";
}

Database::Database(std::filesystem::path databasePath) :
    m_DatabasePath(std::move(databasePath)),
    m_Connection(nullptr)
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

void Database::SaveResultsForSource(const SaveTuningsDto& source) const
{
    OpenOrCreateDatabase();

    auto sourceUdt = SourceRepository::SelectSourceByFingerprints(
        m_Connection,
        source.sourceFingerprint,
        source.parameterFingerprint
    );

    size_t sourceId = 0;

    if (sourceUdt)
    {
        sourceId = sourceUdt->id;
    }
    else
    {
        sourceId = SourceRepository::CreateSource(m_Connection,
            {
                source.parameterFingerprint,
                source.sourceFingerprint
            });
    }

    auto spaceUdt = SpaceRepository::SelectSpaceByFingerprint(m_Connection, sourceId, source.spaceFingerprint);
    size_t spaceId = 0;

    if (spaceUdt)
    {
        spaceId = spaceUdt->id;
    }
    else
    {
        spaceId = SpaceRepository::CreateSpace(m_Connection,
            {
                sourceId,
                source.spaceFingerprint
            });
    }

    const size_t runId = RunRepository::CreateRun(m_Connection);
    ResultRepository::CreateResults(m_Connection, runId, spaceId, source.results);
}

std::vector<KernelResult> Database::LoadBestResultsForSource(const LoadTuningsDto& source) const
{
    OpenOrCreateDatabase();

    const auto sourceUdt = SourceRepository::SelectSourceByFingerprints(m_Connection, source.sourceFingerprint,
        source.parameterFingerprint);
    if (!sourceUdt)
    {
        Logger::LogInfo("No results found for this source");
        return {};
    }

    const auto topResults = ResultRepository::SelectTopResultsForSourceId(m_Connection, sourceUdt->id);
    if (!topResults)
    {
        return {};
    }

    std::vector<KernelResult> output;
    output.reserve(topResults->size());

    for (const auto& resultUdt : *topResults)
    {
        try
        {
            output.push_back(resultUdt.result.get<KernelResult>());
        }
        catch (const std::exception& exception)
        {
            Logger::LogWarning("Failed to deserialize tuning result JSON: " + std::string(exception.what()) + ". Skipping row.");
        }
    }

    return output;
}

} // namespace ktt
