#include <Database/Database.h>
#include <Database/Record.h>
#include <Api/KttException.h>

#include <filesystem>
#include <sqlite3.h>
#include <utility>

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
            constraint_fingerprint INTEGER NOT NULL,
            constraint_undefined_lambda BOOLEAN NOT NULL,
            best_result TEXT NOT NULL,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        );
        CREATE INDEX IF NOT EXISTS idx_fingerprints ON tuning_records(parameter_fingerprint, source_fingerprint);
    )";

    char* errorMsg = nullptr;

    if (const int result = sqlite3_exec(m_Connection, createTableSQL, nullptr, nullptr, &errorMsg); result != SQLITE_OK)
    {
        std::string error = errorMsg ? errorMsg : "Unknown error";
        sqlite3_free(errorMsg);
        throw std::runtime_error("Failed to create table: " + error);
    }
}

// std::unique_ptr<std::vector<Record>> Database::LoadFromDatabase() const
// {
//     OpenOrCreateDatabase();
//
//     auto records = std::make_unique<std::vector<Record>>();
//
//     const char* selectSQL = "SELECT parameter_fingerprint, kernel_source_path, best_result FROM tuning_records";
//     sqlite3_stmt* stmt = nullptr;
//
//     int result = sqlite3_prepare_v2(m_Connection, selectSQL, -1, &stmt, nullptr);
//     if (result != SQLITE_OK)
//     {
//         throw KttException("Failed to prepare SELECT statement: " + std::string(sqlite3_errmsg(m_Connection)),
//                          ExceptionReason::Database);
//     }
//
//     while ((result = sqlite3_step(stmt)) == SQLITE_ROW)
//     {
//         const std::size_t fingerprint = sqlite3_column_int64(stmt, 0);
//         const char* pathStr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
//         const char* jsonStr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
//
//         std::filesystem::path kernelPath(pathStr);
//         json bestResult = json::parse(jsonStr);
//
//         records->emplace_back(fingerprint, kernelPath, bestResult);
//     }
//
//     if (result != SQLITE_DONE)
//     {
//         sqlite3_finalize(stmt);
//         throw KttException("Error reading from database: " + std::string(sqlite3_errmsg(m_Connection)),
//                          ExceptionReason::Database);
//     }
//
//     sqlite3_finalize(stmt);
//     return records;
// }

void Database::WriteToDatabase(const Record& record) const
{
    OpenOrCreateDatabase();

    const char* insertSQL = R"(INSERT INTO tuning_records (parameter_fingerprint, source_fingerprint, constraint_fingerprint, constraint_undefined_lambda, best_result)
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
    sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(record.m_ConstraintFingerprint));
    sqlite3_bind_int(stmt, 4, record.m_HasUndefinedConstraintLambda ? 1 : 0);
    sqlite3_bind_text(stmt, 5, record.m_BestResult.dump().c_str(), -1, SQLITE_STATIC);

    result = sqlite3_step(stmt);
    if (result != SQLITE_DONE)
    {
        std::string error = sqlite3_errmsg(m_Connection);
        sqlite3_finalize(stmt);
        throw KttException("Failed to insert record: " + error, ExceptionReason::Database);
    }

    sqlite3_finalize(stmt);
}

}
