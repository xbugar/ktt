#pragma once
#include <filesystem>
#include <memory>
#include <vector>


#include "Schema/Udts/TuningResultUdt.h"

struct sqlite3;

namespace ktt
{

struct LoadTuningsDto
{
    size_t parameterFingerprint;
    size_t sourceFingerprint;
};

struct SaveTuningsDto
{
    size_t sourceFingerprint;
    size_t parameterFingerprint;
    size_t spaceFingerprint;
    std::vector<KernelResult> results;
};

class Database
{

public:
    explicit Database();

    Database(std::filesystem::path databasePath);

    ~Database();

    // Disable copy
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    void SaveResultsForSource(const SaveTuningsDto& source) const;
    std::vector<KernelResult> LoadBestResultsForSource(const LoadTuningsDto &source) const;

private:
    void OpenOrCreateDatabase() const;
    void CloseDatabase() const;

    std::filesystem::path m_DatabasePath;
    mutable sqlite3* m_Connection;
};
} // namespace ktt
