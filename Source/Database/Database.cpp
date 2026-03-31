#include <filesystem>
#include <iostream>
#include <sqlite3.h>
#include <utility>

#include <Api/KttException.h>
#include <Database/Database.h>
#include <Database/Record.h>
#include <Database/Repository/StatementRepository.h>
#include <Database/Schema/Schema.h>
#include <Database/Schema/Udts/TuningSourceUdt.h>
#include <Utility/Logger/Logger.h>

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

std::unique_ptr<Record> Database::CheckTheDatabase(const Record& data) const
{
    if (auto record = Load(data))
    {
        Logger::LogInfo("Matching record found in database, loading best result from previous tuning session");
        std::cout << "Would you like to use the result from the database? (y/n): ";

        char choice;
        std::cin >> choice;
        if (choice == 'y' || choice == 'Y' || choice == '\n')
            return record;

        return nullptr;
    }
    Logger::LogInfo("No matching record found in the database, proceeding with tuning");
    return nullptr;
}

void Database::SaveToDatabase(const Record &record) const
{
    OpenOrCreateDatabase();

    // Check if a matching record already exists
    auto existingRecord = Load(record);

    if (!existingRecord)
    {
        Logger::LogInfo("No existing record found. Inserting new record into database.");
        Save(record);
        return;
    }


    // Parse both JSON results to KernelResult objects for comparison
    try
    {
        KernelResult newResult;
        KernelResult existingResult;

        from_json(record.m_BestResult, newResult);
        from_json(existingRecord->m_BestResult, existingResult);

        const Nanoseconds newDuration = newResult.GetKernelDuration();
        const Nanoseconds existingDuration = existingResult.GetKernelDuration();

        if (newDuration >= existingDuration)
        {
            Logger::LogInfo("Existing result (duration: " + std::to_string(existingDuration) + " ns) is better than or equal to new result (duration: " + std::to_string(newDuration) + " ns). Keeping existing record.");
            return;
        }

        Logger::LogInfo("New result is better (duration: " + std::to_string(newDuration) + " ns) than existing result (duration: " + std::to_string(existingDuration) + " ns). Updating database.");

        StatementRepository::UpdateTuningRecord(m_Connection, record);
    }
    catch (const json::parse_error& e)
    {
        Logger::LogWarning("Failed to parse JSON for comparison: " + std::string(e.what()) + ". Skipping database update.");
    }
    catch (const std::exception& e)
    {
        Logger::LogWarning("Error during result comparison: " + std::string(e.what()) + ". Skipping database update.");
    }
}




void Database::Save(const Record& record) const
{
    OpenOrCreateDatabase();

    StatementRepository::InsertTuningRecord(m_Connection, record);
}

std::unique_ptr<TuningSourceLoadUdt> Database::LoadBestResultsForSource(const TuningSourceSaveUdt& source) const
{
    OpenOrCreateDatabase();

    auto sourceUdt = StatementRepository::SelectSourceByFingerprints(m_Connection, source);
    if (!sourceUdt)
    {
        return nullptr;
    }

    StatementRepository::SelectTopResultsForSourceId(m_Connection, sourceUdt->id, *sourceUdt);
    return sourceUdt;
}

std::unique_ptr<Record> Database::Load(const Record &data) const
{
    OpenOrCreateDatabase();

    return StatementRepository::SelectTuningRecord(m_Connection, data);
}
} // namespace ktt
