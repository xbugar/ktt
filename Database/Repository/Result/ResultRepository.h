#pragma once
#include <Ktt.h>
#include <memory>
#include <sqlite3.h>
#include <string>
#include <vector>

#include <Repository/Device/DeviceRepository.h>

namespace ktt::db
{

class ResultRepository
{
public:
    static void CreateResults(
        sqlite3 *connection, size_t runId, const std::vector<KernelResult> &results, int indentResultsJson
    );

    static std::vector<KernelResult> SimpleResultQuery(
        sqlite3 *connection, size_t spaceId, const Device &device, uint32_t limit = 50
    );

    static std::vector<KernelResult> ResultsForRunIds(
        sqlite3 *connection, const std::vector<size_t> &runIds, uint32_t limit
    );
};

} // namespace ktt::db
