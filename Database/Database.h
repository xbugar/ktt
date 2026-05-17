#pragma once
#include <KttPlatform.h>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include <Api/Output/KernelResult.h>

struct sqlite3;

namespace ktt
{
struct DatabaseDeviceInfo;
struct DatabaseTuningInfo;
}

namespace ktt::db
{

struct GetResultsQuery
{
    const ktt::DatabaseTuningInfo &source;
    std::function<bool(const DatabaseDeviceInfo &)> devicePredicate;
    std::function<bool(const std::string &)> stringPredicate;
    int limit{50};
};

class KTT_API Database
{

public:
    explicit Database();

    Database(std::filesystem::path databasePath);

    ~Database();

    // Disable copy
    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;

    void SaveResultsForSource(const ktt::DatabaseTuningInfo &source, std::vector<KernelResult> results) const;

    std::vector<KernelResult> LoadBestResultsForSource(const ktt::DatabaseTuningInfo &source, int limit = 50) const;

    std::vector<KernelResult> LoadBestResultsForSourceAndDevice(
        const GetResultsQuery &query
    ) const;

private:
    void OpenOrCreateDatabase() const;
    void CloseDatabase() const;

    std::filesystem::path m_DatabasePath;
    mutable sqlite3 *m_Connection;
};
} // namespace ktt::db
