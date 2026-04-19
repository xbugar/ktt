#include <sqlite3.h>

#include <Api/KttException.h>
#include <Database/Repository/Device/DeviceRepository.h>
#include <Database/Repository/Utility.h>

namespace ktt
{

namespace
{

size_t CreateDevice(sqlite3* connection, const DeviceDescriptor& device)
{
    const char* deviceSql = R"(
        INSERT INTO device (name, vendor, type)
        VALUES (?, ?, ?)
    )";

    sqlite3_stmt* deviceStmt = DatabaseUtility::PrepareStatement(
        connection,
        deviceSql,
        "Failed to prepare device INSERT statement: "
    );

    sqlite3_bind_text(deviceStmt, 1, device.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(deviceStmt, 2, device.vendor.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(deviceStmt, 3, device.type.c_str(), -1, SQLITE_TRANSIENT);

    const int result = sqlite3_step(deviceStmt);

    if (result != SQLITE_DONE)
    {
        const std::string error = sqlite3_errmsg(connection);
        sqlite3_finalize(deviceStmt);
        throw KttException("Failed to execute device INSERT statement: " + error, ExceptionReason::Database);
    }

    sqlite3_finalize(deviceStmt);
    return static_cast<size_t>(sqlite3_last_insert_rowid(connection));
}

void CreateOpenClDeviceDetail(sqlite3* connection, const size_t deviceId, const std::string& extensions)
{
    const char* detailSql = R"(
        INSERT INTO device_open_cl (device_id, extensions)
        VALUES (?, ?)
    )";

    sqlite3_stmt* detailStmt = DatabaseUtility::PrepareStatement(
        connection,
        detailSql,
        "Failed to prepare OpenCL device detail INSERT statement: "
    );

    sqlite3_bind_int64(detailStmt, 1, static_cast<sqlite3_int64>(deviceId));
    sqlite3_bind_text(detailStmt, 2, extensions.c_str(), -1, SQLITE_TRANSIENT);

    const int result = sqlite3_step(detailStmt);

    if (result != SQLITE_DONE)
    {
        const std::string error = sqlite3_errmsg(connection);
        sqlite3_finalize(detailStmt);
        throw KttException("Failed to execute OpenCL device detail INSERT statement: " + error, ExceptionReason::Database);
    }

    sqlite3_finalize(detailStmt);
}

void CreateVulkanDeviceDetail(sqlite3* connection, const size_t deviceId, const std::string& extensions)
{
    const char* detailSql = R"(
        INSERT INTO device_vulkan (device_id, extensions)
        VALUES (?, ?)
    )";

    sqlite3_stmt* detailStmt = DatabaseUtility::PrepareStatement(
        connection,
        detailSql,
        "Failed to prepare Vulkan device detail INSERT statement: "
    );

    sqlite3_bind_int64(detailStmt, 1, static_cast<sqlite3_int64>(deviceId));
    sqlite3_bind_text(detailStmt, 2, extensions.c_str(), -1, SQLITE_TRANSIENT);

    const int result = sqlite3_step(detailStmt);

    if (result != SQLITE_DONE)
    {
        const std::string error = sqlite3_errmsg(connection);
        sqlite3_finalize(detailStmt);
        throw KttException("Failed to execute Vulkan device detail INSERT statement: " + error, ExceptionReason::Database);
    }

    sqlite3_finalize(detailStmt);
}

void CreateCudaDeviceDetail(sqlite3* connection, const size_t deviceId, const int major, const int minor)
{
    const char* detailSql = R"(
        INSERT INTO device_cuda (device_id, version_major, version_minor)
        VALUES (?, ?, ?)
    )";

    sqlite3_stmt* detailStmt = DatabaseUtility::PrepareStatement(
        connection,
        detailSql,
        "Failed to prepare CUDA device detail INSERT statement: "
    );

    sqlite3_bind_int64(detailStmt, 1, static_cast<sqlite3_int64>(deviceId));
    sqlite3_bind_int(detailStmt, 2, major);
    sqlite3_bind_int(detailStmt, 3, minor);

    const int result = sqlite3_step(detailStmt);

    if (result != SQLITE_DONE)
    {
        const std::string error = sqlite3_errmsg(connection);
        sqlite3_finalize(detailStmt);
        throw KttException("Failed to execute CUDA device detail INSERT statement: " + error, ExceptionReason::Database);
    }

    sqlite3_finalize(detailStmt);
}

size_t FindMatchingOpenClDevice(sqlite3* connection, const DeviceDescriptor& device)
{
    const char* findSql = R"(
        SELECT d.id
        FROM device d
        INNER JOIN device_open_cl docl ON docl.device_id = d.id
        WHERE d.name = ? AND d.vendor = ? AND d.type = ? AND docl.extensions = ?
        LIMIT 1
    )";

    sqlite3_stmt* statement = DatabaseUtility::PrepareStatement(
        connection,
        findSql,
        "Failed to prepare OpenCL device SELECT statement: "
    );

    sqlite3_bind_text(statement, 1, device.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, device.vendor.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, device.type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 4, device.extensions.c_str(), -1, SQLITE_TRANSIENT);

    const int result = sqlite3_step(statement);

    if (result == SQLITE_ROW)
    {
        const size_t deviceId = static_cast<size_t>(sqlite3_column_int64(statement, 0));
        sqlite3_finalize(statement);
        return deviceId;
    }

    if (result != SQLITE_DONE)
    {
        const std::string error = sqlite3_errmsg(connection);
        sqlite3_finalize(statement);
        throw KttException("Failed to execute OpenCL device SELECT statement: " + error, ExceptionReason::Database);
    }

    sqlite3_finalize(statement);
    return 0;
}

size_t FindMatchingVulkanDevice(sqlite3* connection, const DeviceDescriptor& device)
{
    const char* findSql = R"(
        SELECT d.id
        FROM device d
        INNER JOIN device_vulkan dv ON dv.device_id = d.id
        WHERE d.name = ? AND d.vendor = ? AND d.type = ? AND dv.extensions = ?
        LIMIT 1
    )";

    sqlite3_stmt* statement = DatabaseUtility::PrepareStatement(
        connection,
        findSql,
        "Failed to prepare Vulkan device SELECT statement: "
    );

    sqlite3_bind_text(statement, 1, device.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, device.vendor.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, device.type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 4, device.extensions.c_str(), -1, SQLITE_TRANSIENT);

    const int result = sqlite3_step(statement);

    if (result == SQLITE_ROW)
    {
        const size_t deviceId = static_cast<size_t>(sqlite3_column_int64(statement, 0));
        sqlite3_finalize(statement);
        return deviceId;
    }

    if (result != SQLITE_DONE)
    {
        const std::string error = sqlite3_errmsg(connection);
        sqlite3_finalize(statement);
        throw KttException("Failed to execute Vulkan device SELECT statement: " + error, ExceptionReason::Database);
    }

    sqlite3_finalize(statement);
    return 0;
}

size_t FindMatchingCudaDevice(sqlite3* connection, const DeviceDescriptor& device)
{
    const char* findSql = R"(
        SELECT d.id
        FROM device d
        INNER JOIN device_cuda dc ON dc.device_id = d.id
        WHERE d.name = ? AND d.vendor = ? AND d.type = ? AND dc.version_major = ?
        LIMIT 1
    )";

    sqlite3_stmt* statement = DatabaseUtility::PrepareStatement(
        connection,
        findSql,
        "Failed to prepare CUDA device SELECT statement: "
    );

    sqlite3_bind_text(statement, 1, device.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, device.vendor.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, device.type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 4, device.cudaComputeCapabilityMajor);

    const int result = sqlite3_step(statement);

    if (result == SQLITE_ROW)
    {
        const size_t deviceId = static_cast<size_t>(sqlite3_column_int64(statement, 0));
        sqlite3_finalize(statement);
        return deviceId;
    }

    if (result != SQLITE_DONE)
    {
        const std::string error = sqlite3_errmsg(connection);
        sqlite3_finalize(statement);
        throw KttException("Failed to execute CUDA device SELECT statement: " + error, ExceptionReason::Database);
    }

    sqlite3_finalize(statement);
    return 0;
}

} // namespace

size_t DeviceRepository::GetOrCreateDevice(sqlite3* connection, const ComputeApi computeApi, const DeviceDescriptor& device)
{
    size_t deviceId = 0;

    switch (computeApi)
    {
        case ComputeApi::OpenCL:
            deviceId = FindMatchingOpenClDevice(connection, device);
            if (deviceId != 0)
            {
                return deviceId;
            }

            deviceId = CreateDevice(connection, device);
            CreateOpenClDeviceDetail(connection, deviceId, device.extensions);
            return deviceId;
            
        case ComputeApi::CUDA:
            deviceId = FindMatchingCudaDevice(connection, device);
            if (deviceId != 0)
            {
                return deviceId;
            }

            deviceId = CreateDevice(connection, device);
            CreateCudaDeviceDetail(connection, deviceId, device.cudaComputeCapabilityMajor, device.cudaComputeCapabilityMinor);
            return deviceId;

        case ComputeApi::Vulkan:
            deviceId = FindMatchingVulkanDevice(connection, device);
            if (deviceId != 0)
            {
                return deviceId;
            }

            deviceId = CreateDevice(connection, device);
            CreateVulkanDeviceDetail(connection, deviceId, device.extensions);
            return deviceId;

        case ComputeApi::Cpp:
            throw KttException("Unsupported compute API for database device persistence", ExceptionReason::Database);
    }

    throw KttException("Unsupported compute API for database device persistence", ExceptionReason::Database);
}

} // namespace ktt

