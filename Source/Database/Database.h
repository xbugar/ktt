#pragma once
#include <filesystem>
#include <iosfwd>
#include <Output/JsonConverters.h>


namespace ktt
{
class TunerCore;
struct Record;

class Database
{
    std::filesystem::path m_DatabasePath;

public:
    explicit Database();

    Database(std::filesystem::path databasePath);

    std::unique_ptr<std::vector<Record>> LoadFromDatabase() const;

    void WriteToDatabase(const Record& record) const;

private:
    std::ifstream OpenOrCreateDatabaseFileForRead() const;
    std::ofstream OpenOrCreateDatabaseFileForWrite() const;

    static inline std::string header = "parameter_finger_print|kernel_source|best_result";
};
} // namespace ktt
