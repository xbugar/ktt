#include <Database/Database.h>
#include <Database/Record.h>

#include <filesystem>
#include <fstream>
#include <utility>

namespace ktt
{

Database::Database()
{
    m_DatabasePath = std::filesystem::path(std::getenv("HOME")) / ".local/share/ktt";
}

Database::Database(std::filesystem::path databasePath) :
    m_DatabasePath(std::move(databasePath))
{
}

std::ifstream Database::OpenOrCreateDatabaseFileForRead() const
{
    std::filesystem::create_directories(m_DatabasePath);

    const auto filePath = m_DatabasePath / "data.csv";
    std::ifstream dbFile(filePath, std::ios::in);

    if (!dbFile.is_open())
        throw std::runtime_error("Failed to open or create database file.");

    return dbFile;
}

std::ofstream Database::OpenOrCreateDatabaseFileForWrite() const
{
    std::filesystem::create_directories(m_DatabasePath);

    const auto filePath = m_DatabasePath / "data.csv";
    std::ofstream dbFile(filePath, std::ios::app);

    if (!dbFile.is_open())
        throw std::runtime_error("Failed to open or create database file.");

    return dbFile;
}

std::unique_ptr<std::vector<Record> > Database::LoadFromDatabase() const
{
    std::ifstream dbFile = OpenOrCreateDatabaseFileForRead();

    if (!dbFile.is_open())
        throw KttException("Failed to open or create database file.", ExceptionReason::Database);

    auto records = std::make_unique<std::vector<Record>>();
    std::string line;
    while (std::getline(dbFile, line))
    {
        std::stringstream ss(line);
        std::string cell;
        Record r = Record::Deserialize(line);

        records->push_back(r);
    }

    dbFile.close();
    return records;
}

void Database::WriteToDatabase(const Record& record) const
{
    std::ofstream dbFile = OpenOrCreateDatabaseFileForWrite();
    if (!dbFile.is_open())
    {
        throw KttException("Database file is not open.", ExceptionReason::Database);
    }
    dbFile << record.Serialize() << std::endl;
    dbFile.close();
}

}
