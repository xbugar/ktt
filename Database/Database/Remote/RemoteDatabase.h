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
     * Not implemented yet.
     * @throw KttException Always.
     */
    void SaveResults(const TuningInfo &source, std::vector<KernelResult> results,
        SaveOptions options = {}) const override;

    /** @fn std::vector<KernelResult> SimpleGetBestResults(const TuningInfo& source, uint32_t limit = 50) const
     * Not implemented yet.
     * @throw KttException Always.
     */
    std::vector<KernelResult> SimpleGetBestResults(const TuningInfo &source, uint32_t limit = 50) const override;

    /** @fn std::vector<KernelResult> GetBestResults(const GetBestResultsQuery& query) const
     * Not implemented yet.
     * @throw KttException Always.
     */
    std::vector<KernelResult> GetBestResults(const GetBestResultsQuery &query) const override;

    /** @fn std::optional<SourceStats> GetStatsForSource(size_t sourceFingerprint) const
     * Not implemented yet.
     * @throw KttException Always.
     */
    std::optional<SourceStats> GetStatsForSource(size_t sourceFingerprint) const override;

private:
    std::unique_ptr<RemoteClient> m_Client;
};

} // namespace ktt::db
