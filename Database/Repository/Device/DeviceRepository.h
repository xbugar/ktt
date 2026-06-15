#pragma once

#include <Ktt.h>
#include <cstddef>
#include <optional>
#include <sqlite3.h>
#include <string>


namespace ktt::db
{

/** @struct DbDeviceInfo
 * Represents device hardware information stored in the database.
 */
struct DbDeviceInfo
{
    std::optional<size_t> id; ///< Unique database identifier.
    std::string name; ///< Device name.
    std::string vendor; ///< Device vendor.
    std::string type; ///< Device type (e.g., "GPU", "CPU").

    /** @fn static DbDeviceInfo FromRow(sqlite3_stmt *stmt)
     * Creates a DbDeviceInfo from a database query result row.
     * @param stmt SQLite prepared statement positioned at a result row.
     * @return DbDeviceInfo populated from the current row.
     */
    static DbDeviceInfo FromRow(sqlite3_stmt *stmt);
};

/** @struct DeviceApi
 * Represents device compute API capabilities and version information.
 */
struct DeviceApi
{
    std::optional<size_t> id; ///< Unique database identifier.
    ComputeApi computeApi; ///< Compute API type (OpenCL, CUDA, etc.).
    std::optional<std::string> extensions; ///< Supported extensions string.
    std::optional<int> cudaComputeCapabilityMajor; ///< CUDA major compute capability.
    std::optional<int> cudaComputeCapabilityMinor; ///< CUDA minor compute capability.

    /** @fn static DeviceApi FromRow(sqlite3_stmt *stmt)
     * Creates a DeviceApi from a database query result row.
     * @param stmt SQLite prepared statement positioned at a result row.
     * @return DeviceApi populated from the current row.
     */
    static DeviceApi FromRow(sqlite3_stmt *stmt);
};

/** @struct Device
 * Complete device information combining hardware and compute API details.
 */
struct Device
{
    std::optional<size_t> id; ///< Unique device identifier.
    std::optional<size_t> apiId; ///< Reference to device_api table.
    std::string name; ///< Device name.
    std::string vendor; ///< Device vendor.
    std::string type; ///< Device type.
    ComputeApi computeApi; ///< Compute API type.
    std::optional<std::string> extensions; ///< Supported extensions.
    std::optional<int> cudaComputeCapabilityMajor; ///< CUDA major compute capability.
    std::optional<int> cudaComputeCapabilityMinor; ///< CUDA minor compute capability.
};

/** @class DeviceRepository
 * Data access layer for device information in the database.
 * Provides methods to create, retrieve, and manage device and device API records.
 */
class DeviceRepository
{
public:
    /** @fn static Device GetOrCreateDevice(sqlite3 *connection, const Device &device)
     * Gets or creates a device record in the database.
     * Attempts to retrieve existing device and API, creates new if not found.
     * @param connection SQLite database connection.
     * @param device Device information to get or create.
     * @return Device struct with populated id and apiId fields.
     */
    static Device GetOrCreateDevice(sqlite3 *connection, const Device &device);

    /** @fn static size_t CreateDevice(sqlite3 *connection, const DbDeviceInfo &device)
     * Creates a new device record in the database.
     * @param connection SQLite database connection.
     * @param device Device information to insert.
     * @return Database ID of the newly created device record.
     * @throw KttException If insertion fails.
     */
    static size_t CreateDevice(sqlite3 *connection, const DbDeviceInfo &device);

    /** @fn static size_t CreateDeviceApi(sqlite3 *connection, const DeviceApi &deviceApi)
     * Creates a new device API record in the database.
     * @param connection SQLite database connection.
     * @param deviceApi Device API information to insert.
     * @return Database ID of the newly created device_api record.
     * @throw KttException If insertion fails.
     */
    static size_t CreateDeviceApi(sqlite3 *connection, const DeviceApi &deviceApi);

    /** @fn static std::optional<DbDeviceInfo> GetDeviceInfo(sqlite3 *connection, const DbDeviceInfo &device)
     * Retrieves device hardware information from the database.
     * Searches for a device by name, vendor, and type.
     * @param connection SQLite database connection.
     * @param device Device information to search for.
     * @return Optional DbDeviceInfo if found, std::nullopt otherwise.
     * @throw KttException If query fails.
     */
    static std::optional<DbDeviceInfo> GetDeviceInfo(sqlite3 *connection, const DbDeviceInfo &device);

    /** @fn static std::optional<DeviceApi> GetDeviceApi(sqlite3 *connection, const DeviceApi &deviceApi)
     * Retrieves device API information from the database.
     * Searches for a device API by compute API type and version information.
     * @param connection SQLite database connection.
     * @param deviceApi Device API information to search for.
     * @return Optional DeviceApi if found, std::nullopt otherwise.
     * @throw KttException If query fails.
     */
    static std::optional<DeviceApi> GetDeviceApi(sqlite3 *connection, const DeviceApi &deviceApi);

    /** @fn static std::optional<DeviceApi> GetDeviceApiForSimpleQuery(sqlite3 *connection, const DeviceApi &deviceApi)
     * Retrieves device API information using relaxed matching criteria.
     * Uses "IS" NULL comparisons for more flexible matching.
     * @param connection SQLite database connection.
     * @param deviceApi Device API information to search for.
     * @return Optional DeviceApi if found, std::nullopt otherwise.
     * @throw KttException If query fails.
     */
    static std::optional<DeviceApi> GetDeviceApiForSimpleQuery(sqlite3 *connection, const DeviceApi &deviceApi);
};

} // namespace ktt::db
