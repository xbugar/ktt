#pragma once
#include <KttPlatform.h>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <Database/IDatabase.h>
#include <Database/Remote/ResultQuery.h>

namespace ktt::db
{

class HttpClient;
class RemoteClient;

/** @struct RemoteDatabaseOptions
 * Connection settings of a RemoteDatabase. A proxy is taken from the environment variables that libcurl reads:
 * http_proxy (lower case only), and https_proxy, all_proxy and no_proxy (in lower or upper case).
 */
struct RemoteDatabaseOptions
{
    std::chrono::milliseconds connectTimeout{5000}; ///< Time limit for establishing the connection (0 = none).
    std::chrono::milliseconds requestTimeout{60000}; ///< Time limit for a whole request, per attempt (0 = none).
    uint32_t maxRetries{3}; ///< Additional attempts after a connection error, a timeout or 502 / 503 / 504.
    bool verifyTls{true}; ///< Verify the server's TLS certificate and host name. Disable only for testing.
    std::optional<std::filesystem::path> caBundle; ///< A custom CA bundle (PEM), e.g. for an internal server.
};

/** @class RemoteDatabase
 * Stores and reads tuning results on a ktt-db server through its application API (HTTP, authenticated with an API
 * key), as an alternative to the local SQLite Database. Available when KTT is built with --remote-database.
 * The API key decides which realms are read and written; there is no realm parameter. Every failure throws
 * KttException, and the API key never appears in exception messages or logs.
 * Requests are retried with exponential backoff after connection errors, timeouts and the statuses 502, 503 and 504
 * (see RemoteDatabaseOptions::maxRetries). The methods are const and can be called from several threads at once.
 * Results are read in the time unit their run was saved with by switching KTT's process-wide time unit for a moment
 * (see TimeUnitGuard); other code that reads or sets the time unit at the same time can see the switched unit.
 */
class KTT_API RemoteDatabase : public IDatabase
{
public:
    /** @fn RemoteDatabase(std::string baseUrl, std::string apiKey, RemoteDatabaseOptions options = {})
     * Creates a client for the server at baseUrl. The constructor does not contact the server.
     * @param baseUrl Server root including the scheme, e.g. https://ktt.example.org. The client appends
     *                /ktt-db/app/... to it; a trailing slash is ignored.
     * @param apiKey API key created in the web app's settings (x-api-key).
     * @param options Timeouts, retries and TLS settings.
     * @throw KttException If baseUrl is not an http:// or https:// URL, or apiKey is empty or contains a line break.
     */
    RemoteDatabase(std::string baseUrl, std::string apiKey, RemoteDatabaseOptions options = {});

    /** @fn RemoteDatabase(std::string baseUrl, std::string apiKey, RemoteDatabaseOptions options,
     * std::shared_ptr<HttpClient> httpClient)
     * Creates a client that sends its requests through the given HTTP client instead of libcurl. HttpClient is an
     * internal type, so this constructor is meant for tests. The timeouts and TLS settings in options are not used.
     * @param baseUrl Server root including the scheme.
     * @param apiKey API key sent with every request.
     * @param options Retry settings.
     * @param httpClient The HTTP client. Must not be null.
     * @throw KttException If baseUrl or apiKey is invalid, or httpClient is null.
     */
    RemoteDatabase(std::string baseUrl, std::string apiKey, RemoteDatabaseOptions options,
        std::shared_ptr<HttpClient> httpClient);

    /** @fn ~RemoteDatabase()
     * Destructor.
     */
    ~RemoteDatabase() override;

    // Disable copy
    RemoteDatabase(const RemoteDatabase &) = delete;
    RemoteDatabase &operator=(const RemoteDatabase &) = delete;

    /** @fn void SaveResults(const TuningInfo& source, std::vector<KernelResult> results, SaveOptions options = {}) const
     * Saves the kernel results of one tuning run on the server (POST /ktt-db/app/run) and links the run to every
     * usable realm of the API key. As locally, only results with status Ok are saved, and a run without results is
     * still created. The run gets a UUIDv7 id generated before the request, so a retried request cannot store the run
     * twice. The payloads are written in the current time unit, which the run records.
     * Differences from Database: OutputFormat::XML is stored as JSON (a warning is logged once per process), and
     * options.indent is ignored. The source and space names are sent with every save, but the server applies them only
     * when the source or space first appears in a realm; it trims them, and a name longer than 100 characters is
     * rejected.
     * @param source The TuningInfo containing source fingerprint, space info, device info, and input data.
     * @param results Kernel results of the run.
     * @param options Output format; indent is not used.
     * @throw KttException If source is invalid (before any request), the server rejects the run (e.g. HTTP 400, 401,
     *        403, 409 or 413), or the request fails after its retries.
     */
    void SaveResults(const TuningInfo &source, std::vector<KernelResult> results,
        SaveOptions options = {}) const override;

    /** @fn std::vector<KernelResult> SimpleGetBestResults(const TuningInfo& source, uint32_t limit = 50) const
     * Retrieves the best stored results for the tuning problem on a matching device (POST /ktt-db/app/result/best):
     * the same device model, or for CUDA the same compute capability major and a minor within +-1, as locally. Only
     * runs the API key can see are searched.
     * @param source The TuningInfo containing source fingerprint, space info, and device info to search for.
     * @param limit Maximum number of results to return, at most 1000. 0 returns no results without a request.
     * @return Results ordered by the stored kernel duration, fastest first; empty for an unknown source or space.
     * @throw KttException If source is invalid, limit is above 1000, or the request fails.
     */
    std::vector<KernelResult> SimpleGetBestResults(const TuningInfo &source, uint32_t limit = 50) const override;

    /** @fn std::vector<KernelResult> GetBestResults(const GetBestResultsQuery& query) const
     * Retrieves the best stored results of the runs selected by the query's predicates. The predicates run on the
     * client, so this takes several requests: the space is looked up, its runs are read in pages of 500, and the
     * results of the selected runs of each page are fetched and merged by kernel duration. As locally, a run without
     * input data is left out when an input predicate is set. A run listed twice (offset paging can shift while runs
     * are being saved) is used once. Only runs the API key can see are searched.
     * @param query Source info, optional device and input-data predicates, and a result limit of at most 1000.
     * @return Results ordered by the stored kernel duration, fastest first, limited to query.limit; empty for an
     *         unknown source or space and for a limit of 0.
     * @throw KttException If query.limit is above 1000 or a request fails. An exception thrown by a predicate
     *        propagates.
     */
    std::vector<KernelResult> GetBestResults(const GetBestResultsQuery &query) const override;

    /** @fn std::optional<SourceStats> GetStatsForSource(size_t sourceFingerprint) const
     * Retrieves aggregate statistics for a tuning source (GET /ktt-db/app/source/{fingerprint}/stats). The counts
     * cover only the runs the API key can see, so they can be lower than the server's totals, and name is always
     * empty: the application API returns no names.
     * @param sourceFingerprint The fingerprint hash identifying the tuning source.
     * @return The statistics, or std::nullopt if the source is not found or the key sees none of its runs.
     * @throw KttException If the request fails.
     */
    std::optional<SourceStats> GetStatsForSource(size_t sourceFingerprint) const override;

    /** @fn std::vector<ResultQueryItem> QueryResults(const ResultQuery& query) const
     * Searches every result the API key can see with an OData-style filter (POST /ktt-db/app/result/query). Remote
     * only: the local Database has no equivalent. The filter is sent unchanged and checked by the server, whose
     * message points at the problem. Unlike GetBestResults, the condition runs on the server in one request, so it is
     * the fast path whenever it can be written as a filter. Page with query.top and query.skip.
     * A query that runs into the server's statement timeout (HTTP 503 with code query_timeout) is not retried,
     * because it would time out again; narrow the filter instead. Other 502 / 503 / 504 responses are retried.
     * @param query Filter, order, page and whether to include the stored payloads.
     * @return One page of results in the requested order; empty when nothing matches or query.top is 0.
     * @throw KttException If query.top is above 1000 or query.skip above 100000 (before any request), the server
     *        rejects the query (e.g. HTTP 400 for a bad filter, with the server's message), the query times out, or
     *        the request fails after its retries.
     */
    std::vector<ResultQueryItem> QueryResults(const ResultQuery &query) const;

private:
    std::unique_ptr<RemoteClient> m_Client;
};

} // namespace ktt::db
