#pragma once

#include <cstddef>
#include <optional>
#include <sqlite3.h>
#include <vector>

#include <Api/Info/DatabaseTuningInfo.h>

namespace ktt::db
{

/** @struct Run
 * Represents a kernel tuning run linking a device to a parameter space.
 */
struct Run
{
    std::optional<size_t> id; ///< Unique database identifier.
    size_t spaceId; ///< Reference to tuning_space.
    size_t deviceId; ///< Reference to device_info.
    size_t deviceApiId; ///< Reference to device_api.
    std::optional<std::string> inputData; ///< Optional input data for the run.
};

/** @struct RunQueryResult
 * Complete run information retrieved from a database query.
 */
struct RunQueryResult
{
    size_t runId{}; ///< Database ID of the run.
    std::optional<std::string> inputData; ///< Input data associated with the run.
    DeviceInfo deviceInfo{}; ///< Device details.
};

/** @class RunRepository
 * Data access layer for tuning runs in the database.
 * Manages creation and retrieval of tuning run records that link devices to parameter spaces.
 */
class RunRepository
{
public:
    /** @fn static size_t CreateRun(sqlite3 *connection, const Run &run)
     * Creates a new run record in the database.
     * @param connection SQLite database connection.
     * @param run Run information to insert.
     * @return Database ID of the newly created run.
     * @throw KttException If insertion fails.
     */
    static size_t CreateRun(sqlite3 *connection, const Run &run);

    /** @fn static std::vector<RunQueryResult> GetRunsForSpacePaged(sqlite3 *connection, size_t spaceId, size_t offset,
     * size_t limit) Retrieves runs for a tuning space with pagination support. Useful for processing large result sets
     * in batches.
     * @param connection SQLite database connection.
     * @param spaceId Database ID of the tuning space.
     * @param offset Number of results to skip (for pagination).
     * @param limit Maximum number of results to return. If 0, returns all results.
     * @return Vector of RunQueryResult objects, ordered by run ID.
     * @throw KttException If query fails.
     */
    static std::vector<RunQueryResult> GetRunsForSpacePaged(
        sqlite3 *connection, size_t spaceId, size_t offset, size_t limit
    );
};

} // namespace ktt::db
