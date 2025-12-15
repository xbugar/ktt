#pragma once
#include <filesystem>
#include <iosfwd>
#include <Output/JsonConverters.h>


namespace ktt
{
struct Record;

class Database
{
    std::filesystem::path m_DatabasePath;

public:
    explicit Database();

    explicit Database(std::filesystem::path databasePath);

    static std::ofstream OpenOrCreateDatabaseFile();

    [[nodiscard]]
    std::unique_ptr<std::vector<Record>> LoadFromDatabase() const;

    static void WriteToDatabase(std::ofstream& dbFile, const std::size_t& data);
};
}
