#pragma once
#include <Ktt.h>
#include <memory>
#include <sqlite3.h>
#include <string>
#include <vector>

#include <Repository/Device/DeviceRepository.h>

namespace ktt::db
{

/** @class ResultRepository
 * Data access layer for kernel execution results in the database.
 * Manages storage and retrieval of kernel execution results with performance metrics.
 */
class ResultRepository
{
public:
    /** @fn static void CreateResults(sqlite3 *connection, size_t runId, const std::vector<KernelResult> &results, int
     * indentResultsJson) Stores kernel execution results in the database. Inserts successful results (ResultStatus::Ok)
     * as JSON serialized data.
     * @param connection SQLite database connection.
     * @param runId Database ID of the run these results belong to.
     * @param results Vector of KernelResult objects to store.
     * @param indentResultsJson Indentation level for JSON serialization.
     * @throw KttException If insertion fails.
     */
    static void CreateResults(
        sqlite3 *connection, size_t runId, const std::vector<KernelResult> &results, int indentResultsJson
    );

    /** @fn static std::vector<KernelResult> SimpleResultQuery(sqlite3 *connection, size_t spaceId, const Device
     * &device, uint32_t limit = 50) Retrieves the best kernel results for a space and device combination. Results are
     * ordered by duration (fastest first). Supports device matching by exact name/vendor/type or by compute API and
     * version.
     * @param connection SQLite database connection.
     * @param spaceId Database ID of the tuning space.
     * @param device Device information to filter by.
     * @param limit Maximum number of results to return (default 50).
     * @return Vector of KernelResult objects sorted by execution time.
     * @throw KttException If query fails.
     */
    static std::vector<KernelResult> SimpleResultQuery(
        sqlite3 *connection, size_t spaceId, const Device &device, uint32_t limit = 50
    );

    /** @fn static std::vector<KernelResult> ResultsForRunIds(sqlite3 *connection, const std::vector<size_t> &runIds,
     * uint32_t limit) Retrieves kernel results for a list of run IDs.
     * @param connection SQLite database connection.
     * @param runIds Vector of run IDs to retrieve results for.
     * @param limit Maximum number of results to return.
     * @return Vector of KernelResult objects sorted by execution time.
     * @throw KttException If query fails or runIds is empty.
     */
    static std::vector<KernelResult> ResultsForRunIds(
        sqlite3 *connection, const std::vector<size_t> &runIds, uint32_t limit
    );
};

} // namespace ktt::db
