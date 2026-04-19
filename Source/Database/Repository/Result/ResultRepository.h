#pragma once
#include <memory>
#include <sqlite3.h>
#include <string>
#include <vector>

#include <ComputeEngine/ComputeApi.h>
#include <Database/Schema/Udts/TuningResultUdt.h>

namespace ktt
{

struct CompatibleResultQuery
{
    ComputeApi computeApi;
    std::string deviceExtensions;
    int cudaComputeCapabilityMajor;
    int limit;
};

class ResultRepository
{
    public:

    static void CreateResults(
        sqlite3* connection,
        size_t runId,
        const std::vector<KernelResult>& results);

    static std::vector<KernelResult> SelectCompatibleBestResultsForSourceId(
        sqlite3* connection,
        size_t sourceId,
        const CompatibleResultQuery& query
    );
};

} // namespace ktt
