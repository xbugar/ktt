#pragma once

#include <cstddef>
#include <sqlite3.h>

namespace ktt::db
{

/** @class DatabaseSync
 * Copies tuning data from one database connection into another.
 * Runs are identified by their GUID, so a run that already exists in the target is skipped, which makes
 * syncing idempotent and safe to repeat. Each newly copied run brings along its tuning source, tuning space,
 * device and results.
 */
class DatabaseSync
{
public:
    /** @fn static size_t SyncRuns(sqlite3 *target, sqlite3 *source)
     * Copies every run that is missing from the target database (by GUID) from the source database.
     * The source, space and device each run depends on are resolved with get-or-create semantics in the
     * target, and the run's results are copied verbatim. All inserts run inside a single transaction.
     * @param target Connection to the database that receives the data.
     * @param source Connection to the database the data is read from.
     * @return Number of runs newly inserted into the target.
     * @throw KttException If the sync fails; the target transaction is rolled back.
     */
    static size_t SyncRuns(sqlite3 *target, sqlite3 *source);
};

} // namespace ktt::db
