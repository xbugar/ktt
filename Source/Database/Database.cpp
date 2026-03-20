#include <filesystem>
#include <iostream>
#include <sqlite3.h>
#include <utility>

#include <Api/KttException.h>
#include <Database/Database.h>
#include <Database/Record.h>
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
    const int result = sqlite3_open(filePath.string().c_str(), &m_Connection);

    if (result != SQLITE_OK)
    {
        std::string error = sqlite3_errmsg(m_Connection);
        sqlite3_close(m_Connection);
        m_Connection = nullptr;
        throw std::runtime_error("Failed to open or create database: " + error);
    }

    CreateTableIfNotExists();
}

void Database::CloseDatabase() const
{
    if (m_Connection != nullptr)
    {
        sqlite3_close(m_Connection);
        m_Connection = nullptr;
    }
}

void Database::CreateTableIfNotExists() const
{
    const auto createTableSQL = R"(
        CREATE TABLE IF NOT EXISTS tuning_records (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            parameter_fingerprint INTEGER NOT NULL,
            source_fingerprint INTEGER NOT NULL,
            tuning_space_fingerprint INTEGER NOT NULL,
            gpu_architecture TEXT NOT NULL,
            best_result TEXT NOT NULL,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        );
        CREATE INDEX IF NOT EXISTS idx_fingerprints ON tuning_records(parameter_fingerprint, source_fingerprint, tuning_space_fingerprint);
    )";

    char* errorMsg = nullptr;

    if (const int result = sqlite3_exec(m_Connection, createTableSQL, nullptr, nullptr, &errorMsg); result != SQLITE_OK)
    {
        std::string error = errorMsg ? errorMsg : "Unknown error";
        sqlite3_free(errorMsg);
        throw std::runtime_error("Failed to create table: " + error);
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

        // Update the existing record
        const char* updateSQL = R"(UPDATE tuning_records
                                    SET best_result = ?, created_at = CURRENT_TIMESTAMP
                                    WHERE parameter_fingerprint = ? AND source_fingerprint = ? AND tuning_space_fingerprint = ? AND gpu_architecture = ?)";
        sqlite3_stmt* stmt = nullptr;

        int result = sqlite3_prepare_v2(m_Connection, updateSQL, -1, &stmt, nullptr);
        if (result != SQLITE_OK)
        {
            throw KttException("Failed to prepare UPDATE statement: " + std::string(sqlite3_errmsg(m_Connection)),
                                ExceptionReason::Database);
        }

        const std::string jsonString = record.m_BestResult.dump();
        sqlite3_bind_text(stmt, 1, jsonString.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(record.m_ParameterFingerprint));
        sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(record.m_SourceFingerprint));
        sqlite3_bind_int64(stmt, 4, static_cast<sqlite3_int64>(record.m_TuningSpaceFingerprint));
        sqlite3_bind_text(stmt, 5, record.m_Gpu.c_str(), -1, SQLITE_STATIC);

        result = sqlite3_step(stmt);
        if (result != SQLITE_DONE)
        {
            std::string error = sqlite3_errmsg(m_Connection);
            sqlite3_finalize(stmt);
            throw KttException("Failed to update record: " + error, ExceptionReason::Database);
        }

        sqlite3_finalize(stmt);
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

    const char* insertSQL = R"(INSERT INTO tuning_records
                                (parameter_fingerprint, source_fingerprint, tuning_space_fingerprint, gpu_architecture, best_result)
                                VALUES (?, ?, ?, ?, ?))";
    sqlite3_stmt* stmt = nullptr;

    int result = sqlite3_prepare_v2(m_Connection, insertSQL, -1, &stmt, nullptr);
    if (result != SQLITE_OK)
    {
        throw KttException("Failed to prepare INSERT statement: " + std::string(sqlite3_errmsg(m_Connection)),
                         ExceptionReason::Database);
    }

    sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(record.m_ParameterFingerprint));
    sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(record.m_SourceFingerprint));
    sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(record.m_TuningSpaceFingerprint));
    sqlite3_bind_text(stmt, 4, record.m_Gpu.c_str(), -1, SQLITE_STATIC);

    const std::string jsonString = record.m_BestResult.dump();
    sqlite3_bind_text(stmt, 5, jsonString.c_str(), -1, SQLITE_TRANSIENT);

    result = sqlite3_step(stmt);
    if (result != SQLITE_DONE)
    {
        std::string error = sqlite3_errmsg(m_Connection);
        sqlite3_finalize(stmt);
        throw KttException("Failed to insert record: " + error, ExceptionReason::Database);
    }

    sqlite3_finalize(stmt);
}

std::unique_ptr<Record> Database::Load(const Record &data) const
{
    OpenOrCreateDatabase();

    const char *selectSQL =
        R"(SELECT parameter_fingerprint, source_fingerprint, tuning_space_fingerprint, gpu_architecture, best_result
                                FROM tuning_records
                                WHERE parameter_fingerprint = ? AND source_fingerprint = ? AND tuning_space_fingerprint = ? AND gpu_architecture = ?
                                ORDER BY created_at DESC
                                LIMIT 1)";
    sqlite3_stmt *stmt = nullptr;

    int result = sqlite3_prepare_v2(m_Connection, selectSQL, -1, &stmt, nullptr);
    if (result != SQLITE_OK)
    {
        throw KttException("Failed to prepare SELECT statement: " + std::string(sqlite3_errmsg(m_Connection)),
                           ExceptionReason::Database);
    }

    sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(data.m_ParameterFingerprint));
    sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(data.m_SourceFingerprint));
    sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(data.m_TuningSpaceFingerprint));
    sqlite3_bind_text(stmt, 4, data.m_Gpu.c_str(), -1, SQLITE_STATIC);

    result = sqlite3_step(stmt);

    if (result == SQLITE_ROW)
    {
        auto record = std::make_unique<Record>();
        record->m_ParameterFingerprint = static_cast<std::size_t>(sqlite3_column_int64(stmt, 0));
        record->m_SourceFingerprint = static_cast<std::size_t>(sqlite3_column_int64(stmt, 1));
        record->m_TuningSpaceFingerprint = static_cast<std::size_t>(sqlite3_column_int64(stmt, 2));
        record->m_Gpu = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));

        const char *jsonText = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 4));
        if (jsonText != nullptr)
        {
            try
            {
                record->m_BestResult = json::parse(jsonText);
            } catch (const json::parse_error &e)
            {
                sqlite3_finalize(stmt);
                Logger::LogWarning("Failed to parse JSON from database: "
                    + std::string(e.what())
                    + ". Returning nullptr.");
                return nullptr;
            }
        }

        sqlite3_finalize(stmt);
        return record;
    }
    if (result == SQLITE_DONE)
    {
        // No matching record found
        sqlite3_finalize(stmt);
        return nullptr;
    }
    std::string error = sqlite3_errmsg(m_Connection);
    sqlite3_finalize(stmt);
    throw KttException("Failed to execute SELECT statement: " + error, ExceptionReason::Database);
}
} // namespace ktt
