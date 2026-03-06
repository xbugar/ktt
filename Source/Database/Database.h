#pragma once
#include <filesystem>
#include <memory>
#include <Output/JsonConverters.h>

struct sqlite3;

namespace ktt
{
class TunerCore;
struct Record;

class Database
{

public:
    explicit Database();

    Database(std::filesystem::path databasePath);

    ~Database();

    // Disable copy
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    std::unique_ptr<std::vector<Record>> LoadFromDatabase() const;

    void WriteToDatabase(const Record& record) const;

private:
    void OpenOrCreateDatabase() const;
    void CloseDatabase() const;
    void CreateTableIfNotExists() const;


    std::filesystem::path m_DatabasePath;
    mutable sqlite3* m_Connection;
};
} // namespace ktt
