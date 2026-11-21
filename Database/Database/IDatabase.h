#pragma once
#include <KttPlatform.h>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <Api/Info/DatabaseTuningInfo.h>
#include <Api/Output/KernelResult.h>
#include <Output/OutputFormat.h>

namespace ktt::db
{

/** @struct GetBestResultsQuery
 * Query for IDatabase::GetBestResults: the tuning problem, optional device and input-data predicates, and a limit.
 */
struct GetBestResultsQuery
{
    const TuningSpaceInfo &source; ///< Source and space fingerprints of the tuning problem.
    std::optional<std::function<bool(const DeviceInfo &)>> devicePredicate; ///< Keeps only runs on matching devices.
    std::optional<std::function<bool(const std::string &)>> inputPredicate; ///< Keeps only runs with matching input data.
    uint32_t limit{50}; ///< Maximum number of results returned.
};

/** @struct SourceStats
 * Holds aggregate statistics for a tuning source.
 */
struct SourceStats
{
    size_t spaceCount{}; ///< Number of tuning spaces for the source.
    size_t deviceCount{}; ///< Number of unique devices used with the source.
    size_t runCount{}; ///< Total number of tuning runs for the source.
    size_t resultCount{}; ///< Total number of kernel results for the source.
    std::optional<std::string> name{}; ///< Human-readable name of the source, if one was set when it was created.
};

/** @struct SaveOptions
 * Options controlling how kernel results are serialized when saved.
 */
struct SaveOptions
{
    ktt::OutputFormat format{ktt::OutputFormat::JSON}; ///< Output format used to serialize stored results.
    int indent{2}; ///< Indentation level applied to JSON output (ignored for XML).
};

/** @class IDatabase
 * Common interface of the tuning result stores: the local SQLite Database and, when KTT is built with
 * --remote-database, the server-backed RemoteDatabase. Code that takes a const IDatabase& works with either.
 * Every failure throws KttException.
 * Default arguments of virtual functions bind to the static type of the call, so every implementation uses the same
 * defaults as this interface (SaveOptions{} and a limit of 50).
 * Operations that exist on one implementation only, such as Database::SyncFrom, are not part of the interface.
 */
class KTT_API IDatabase
{
public:
    /** @fn virtual ~IDatabase()
     * Virtual destructor, so that an implementation can be destroyed through an IDatabase pointer.
     */
    virtual ~IDatabase() = default;

    /** @fn virtual void SaveResults(const TuningInfo& source, std::vector<KernelResult> results, SaveOptions options = {}) const
     * Saves the kernel results of one tuning run for the tuning problem and device described by source.
     * @param source The TuningInfo containing source fingerprint, space info, device info, and input data.
     * @param results Vector of KernelResult objects containing execution data to be stored.
     * @param options Output format and JSON indentation used to serialize the results.
     */
    virtual void SaveResults(const TuningInfo &source, std::vector<KernelResult> results,
        SaveOptions options = {}) const = 0;

    /** @fn virtual std::vector<KernelResult> SimpleGetBestResults(const TuningInfo& source, uint32_t limit = 50) const
     * Retrieves the best (fastest) stored results for the tuning problem on a matching device.
     * @param source The TuningInfo containing source fingerprint, space info, and device info to search for.
     * @param limit Maximum number of results to return. Default is 50.
     * @return Results ordered by the stored kernel duration, fastest first.
     */
    virtual std::vector<KernelResult> SimpleGetBestResults(const TuningInfo &source, uint32_t limit = 50) const = 0;

    /** @fn virtual std::vector<KernelResult> GetBestResults(const GetBestResultsQuery& query) const
     * Retrieves the best stored results of the runs selected by the query's device and input-data predicates.
     * @param query Source info, optional device and input-data predicates, and a result limit.
     * @return Results ordered by the stored kernel duration, fastest first, limited to query.limit.
     */
    virtual std::vector<KernelResult> GetBestResults(const GetBestResultsQuery &query) const = 0;

    /** @fn virtual std::optional<SourceStats> GetStatsForSource(size_t sourceFingerprint) const
     * Retrieves aggregate statistics for a tuning source.
     * @param sourceFingerprint The fingerprint hash identifying the tuning source.
     * @return The statistics, or std::nullopt if the source is not found.
     */
    virtual std::optional<SourceStats> GetStatsForSource(size_t sourceFingerprint) const = 0;
};

} // namespace ktt::db
