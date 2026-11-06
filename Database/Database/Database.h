#pragma once
#include <KttPlatform.h>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include <Api/Info/DatabaseTuningInfo.h>
#include <Api/Output/KernelResult.h>
#include <Output/OutputFormat.h>

struct sqlite3;

/** @namespace ktt::db
 * All classes, methods and type aliases related to KTT Database are located inside the ktt::db namespace.
 */
namespace ktt::db
{

struct DeviceInfo;
struct TuningInfo;
struct TuningSpaceInfo;


struct GetBestResultsQuery
{
    const TuningSpaceInfo &source;
    std::optional<std::function<bool(const DeviceInfo &)>> devicePredicate;
    std::optional<std::function<bool(const std::string &)>> inputPredicate;
    uint32_t limit{50};
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

/** @class Database
 * Manages storage and retrieval of kernel tuning results in a SQLite database.
 * Provides functionality to save execution results, query best performing configurations,
 * and retrieve statistics about tuning sources.
 */
class KTT_API Database
{

public:
    /** @fn Database()
     * Constructs a Database instance with the default database location.
     * The default location is %LOCALAPPDATA%\ktt\ktt.db on Windows and $XDG_DATA_HOME/ktt/ktt.db (falling back to
     * ~/.local/share/ktt/ktt.db) elsewhere. Creates the directory structure if it doesn't exist. Opens the file as
     * Database(std::filesystem::path) does.
     * @throw KttException If the default location cannot be determined (the environment variables above are unset).
     */
    explicit Database();

    /** @fn Database(std::filesystem::path databasePath)
     * Constructs a Database instance with a custom database file path.
     * Opening the file writes to it: the file is created if it does not exist, missing tables and indexes are
     * created, and the compute API rows are inserted when missing, which needs write access even when nothing
     * changes. The file must therefore be writable, also when it is only opened as the source of SyncFrom. Files
     * written with an older schema version are not supported and must be recreated.
     * @param databasePath The filesystem path where the database should be stored.
     */
    Database(std::filesystem::path databasePath);

    /** @fn Database(sqlite3* connection)
     * Constructs a Database instance that operates on an already-open SQLite connection.
     * The connection is adopted but not owned: it stays open after this Database is destroyed and it
     * remains the caller's responsibility to close. Foreign key enforcement is enabled and the schema is
     * created if it does not already exist, so the connection is ready for use. A database written with an
     * older schema version is not supported.
     * @param connection An open SQLite connection handle. Must not be null.
     * @throw KttException If the connection is null.
     */
    explicit Database(sqlite3 *connection);

    /** @fn ~Database()
     * Destructor that closes the database connection and releases resources.
     */
    ~Database();

    // Disable copy
    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;

    /** @fn void SaveResults(const TuningInfo& source, std::vector<KernelResult> results, SaveOptions options = {})
     * const Saves kernel execution results for a specific tuning source and configuration. Stores results in the
     * database, organizing them by source fingerprint, tuning space, device, and run information. Creates new records
     * in the database schema if they don't exist. The source and space names in TuningInfo::spaceInfo are stored only
     * when the source or space is created; an existing source or space keeps its name.
     * @param source The TuningInfo containing source fingerprint, space info, device info, and input data.
     * @param results Vector of KernelResult objects containing execution data to be stored.
     * @param options Output format and JSON indentation used to serialize the results.
     *                Defaults to JSON with an indentation of 2.
     */
    void SaveResults(const TuningInfo &source, std::vector<KernelResult> results, SaveOptions options = {}) const;

    /** @fn std::vector<KernelResult> SimpleGetBestResults(const TuningInfo& source, uint32_t limit = 50) const
     * Retrieves the best (fastest) kernel results for a given tuning source and configuration.
     * Queries the database for the best execution results matching the specified source and device.
     * @param source The TuningInfo containing source fingerprint, space info, and device info to search for.
     * @param limit Maximum number of results to return. Default is 50.
     * @return Vector of KernelResult objects sorted by execution time (fastest first).
     */
    std::vector<KernelResult> SimpleGetBestResults(const TuningInfo &source, uint32_t limit = 50) const;

    /** @fn std::vector<KernelResult> GetBestResults(const GetBestResultsQuery& query) const
     * Retrieves the best kernel results using advanced query filters and predicates.
     * Allows complex queries with device and string (input data) predicates for fine-grained filtering.
     * Results are paginated internally and sorted by execution time.
     * @param query A GetBestResultsQuery struct containing source info, optional device and string predicates,
     *              and a result limit.
     * @return Vector of KernelResult objects sorted by execution time, limited to the specified count.
     */
    std::vector<KernelResult> GetBestResults(const GetBestResultsQuery &query) const;

    /** @fn std::optional<SourceStats> GetStatsForSource(size_t sourceFingerprint) const
     * Retrieves statistical information for a given tuning source.
     * Computes and returns counts of tuning spaces, devices, runs, and results associated with a source, together
     * with the source's name.
     * @param sourceFingerprint The fingerprint hash identifying the tuning source.
     * @return Optional SourceStats containing counts and the name (std::nullopt when the source has none), or
     *         std::nullopt if source not found.
     */
    std::optional<SourceStats> GetStatsForSource(size_t sourceFingerprint) const;

    /** @fn size_t SyncFrom(const Database& other) const
     * Copies all tuning data from another Database into this database.
     * Every run the other database contains is copied over, together with its tuning source, tuning space, device
     * and results. Source and space names are copied with them, but only applied when the source or space is new
     * in this database. The other Database may have been constructed from a path, an in-memory database or an existing
     * connection. SyncFrom only reads runs from it, but constructing it from a path writes to the file (it creates
     * the schema when missing), so that file must be writable (see Database(std::filesystem::path)). Runs are
     * matched by their GUID, so runs that already exist in this database are skipped; this makes the operation
     * idempotent and safe to repeat. The copy runs inside a single transaction, so a failure leaves this database
     * unchanged.
     * @param other The Database to sync data from.
     * @return Number of runs newly added to this database.
     * @throw KttException If other is this database (or operates on the same file), or the sync fails.
     */
    size_t SyncFrom(const Database &other) const;

private:
    static constexpr size_t RunBatchSize = 500;

    /** @fn void OpenOrCreateDatabase() const
     * Opens an existing database connection or creates a new one if it doesn't exist.
     * Initializes the SQLite connection, enables foreign key constraints, and creates the database
     * schema if necessary.
     */
    void OpenOrCreateDatabase() const;

    /** @fn void CloseDatabase() const
     * Closes the current database connection and releases resources.
     */
    void CloseDatabase() const;

    std::filesystem::path m_DatabasePath;
    mutable sqlite3 *m_Connection;
    bool m_OwnsConnection{true}; ///< Whether this instance owns Connection and must close it on destruction.
};
} // namespace ktt::db
