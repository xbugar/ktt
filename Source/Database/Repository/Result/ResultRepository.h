#pragma once
#include <memory>
#include <sqlite3.h>
#include <vector>

#include <Database/Schema/Udts/TuningResultUdt.h>

namespace ktt
{

class ResultRepository
{
    public:

    static void CreateResults(
        sqlite3* connection,
        size_t runId,
        size_t spaceId,
        const std::vector<KernelResult>& results);

    static std::unique_ptr<std::vector<TuningResultLoadUdt>> SelectTopResultsForSourceId(
        sqlite3 *connection,
        size_t sourceId,
        size_t limit = 5
    );
};

} // namespace ktt
