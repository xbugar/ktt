#pragma once
#include <KttPlatform.h>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include <Api/Info/DatabaseTuningInfo.h>
#include <Api/Output/KernelResult.h>
#include <Utility/JsonConverters.h>

struct sqlite3;

namespace ktt::db
{

struct DeviceInfo;
struct TuningInfo;
struct TuningSpaceInfo;

struct GetResultsQuery
{
    const TuningSpaceInfo &source;
    std::optional<std::function<bool(const DeviceInfo &)>> devicePredicate;
    std::optional<std::function<bool(const std::string &)>> stringPredicate;
    uint32_t limit{50};
};

struct SourceStats
{
    size_t spaceCount{};
    size_t deviceCount{};
    size_t runCount{};
    size_t resultCount{};
};

class KTT_API Database
{

public:
    explicit Database(int indentResultsJson = 2);

    Database(std::filesystem::path databasePath, int indentResultsJson = 2);

    ~Database();

    // Disable copy
    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;

    void SaveResultsForSource(const TuningInfo &source, std::vector<KernelResult> results) const;

    std::vector<KernelResult> GetBestResultsForSource(const TuningInfo &source, uint32_t limit = 50) const;

    std::vector<KernelResult> GetBestResultsQuery(const GetResultsQuery &query) const;

    std::optional<SourceStats> GetStatsForSource(size_t sourceFingerprint) const;

private:
    static constexpr size_t RunBatchSize = 500;

    void OpenOrCreateDatabase() const;
    void CloseDatabase() const;

    std::filesystem::path DatabasePath;
    mutable sqlite3 *Connection;
    int IndentResultsJson;
};
} // namespace ktt::db
