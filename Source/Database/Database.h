#pragma once
#include <filesystem>
#include <memory>

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

    std::unique_ptr<Record> CheckTheDatabase(const Record& data) const;
    void SaveToDatabase(const Record &record) const;

private:
    void OpenOrCreateDatabase() const;
    void CloseDatabase() const;
    void CreateTableIfNotExists() const;

    void Save(const Record &record) const;
    std::unique_ptr<Record> Load(const Record &data) const;

    std::filesystem::path m_DatabasePath;
    mutable sqlite3* m_Connection;
};
} // namespace ktt
