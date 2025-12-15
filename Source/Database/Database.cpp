#include "Database.h"

#include <fstream>
#include <utility>

#include "Record.h"

namespace ktt
{

Database::Database()
{
    m_DatabasePath = std::filesystem::path(std::getenv("HOME")) / ".local/share/ktt";
}

Database::Database(std::filesystem::path databasePath) :
    m_DatabasePath(std::move(databasePath)) {}

std::ofstream Database::OpenOrCreateDatabaseFile()
{
    auto dir = std::filesystem::path(std::getenv("HOME")) / ".local/share/ktt";
    std::filesystem::create_directories(dir);

    auto file_path = dir / "data.db";
    std::ofstream dbFile(file_path, std::ios::app);

    if (!dbFile.is_open())
        throw std::runtime_error("Failed to open or create database file.");

    return dbFile;
}

std::unique_ptr<std::vector<Record>> Database::LoadFromDatabase() const
{
    auto file_path = m_DatabasePath / "data.db";
    std::ifstream dbFile(file_path, std::ios::in);

    if (!dbFile.is_open())
        throw std::runtime_error("Failed to open or create database file.");

    std::unique_ptr<std::vector<Record>> records;
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

void Database::WriteToDatabase(std::ofstream& dbFile, const std::size_t& data)
{
    if (!dbFile.is_open())
    {
        throw std::runtime_error("Database file is not open.");
    }
    dbFile << data << std::endl;
}

}
