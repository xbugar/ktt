#pragma once
#include <cstdint>
#include <map>
#include <optional>
#include <string>

#include <Api/Info/DatabaseTuningInfo.h>
#include <Api/Output/KernelResult.h>
#include <KttTypes.h>

namespace ktt::db
{

/** @struct ResultQuery
 * Query for RemoteDatabase::QueryResults: an OData-style filter over every result the API key can see, the order and
 * one page. The filter language (fields, operators, literals and limits) is specified in the ktt-db docs, query.md.
 */
struct ResultQuery
{
    std::string filter; ///< Filter, e.g. "device/compute_api eq 'cuda' and configuration/BLOCK_SIZE ge 32". Sent unchanged; empty matches every result.
    std::string orderBy{"duration asc"}; ///< Comma-separated "field [asc|desc]", at most 3 sortable fields. The server appends "id asc".
    uint64_t top{50}; ///< Page size, at most 1000. 0 returns no items without a request.
    uint64_t skip{0}; ///< Number of results skipped before the page, at most 100000.
    bool includeResult{true}; ///< Also return the stored payload, deserialized into ResultQueryItem::result.
};

/** @struct ResultQueryItem
 * One result found by RemoteDatabase::QueryResults, with its run, device and source.
 */
struct ResultQueryItem
{
    std::optional<KernelResult> result; ///< The stored payload, read in its run's time unit; std::nullopt when ResultQuery::includeResult is false.
    uint64_t duration{}; ///< Kernel duration in nanoseconds, as stored on save (what the server sorts and filters by).
    uint64_t totalDuration{}; ///< Total duration in nanoseconds, as stored on save.
    std::map<std::string, ParameterValue> configuration; ///< Tuning parameters. JSON carries no integer type, so a non-negative integer is read as uint64_t and a negative one as int64_t.
    std::string runId; ///< The run's UUID, lowercase; version 7 for runs saved through the server.
    std::optional<std::string> inputData; ///< The run's input data, std::nullopt when it has none.
    DeviceInfo device; ///< The device of the run.
    std::string sourceFingerprint; ///< Fingerprint of the tuning source in decimal. Names are not returned.
};

} // namespace ktt::db
