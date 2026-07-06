#include "DeviceRepository.h"

#include <sqlite3.h>

#include <Api/KttException.h>
#include <Database/Repository/Device/DeviceRepository.h>
#include <Database/Repository/Utility.h>

namespace ktt::db
{
size_t DeviceRepository::CreateDevice(sqlite3 *connection, const dbDeviceInfo &device)
{
    const char *deviceSql = R"(
        INSERT INTO device_info
        (name, vendor, type)
        VALUES (?, ?, ?)
    )";

    sqlite3_stmt *deviceStmt = DatabaseUtility::PrepareStatement(
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
        throw KttException("Failed to execute device INSERT statement: " + error);
    }

    sqlite3_finalize(deviceStmt);
    return static_cast<size_t>(sqlite3_last_insert_rowid(connection));
}

std::optional<dbDeviceInfo> DeviceRepository::GetDeviceInfo(sqlite3 *connection, const dbDeviceInfo &device)
{
    const char *deviceSql = R"(
        SELECT id, name, vendor, type
        FROM device_info
        WHERE name = ? AND vendor = ? AND type = ?
        LIMIT 1
    )";

    sqlite3_stmt *deviceStmt = DatabaseUtility::PrepareStatement(
        connection,
        deviceSql,
        "Failed to prepare device SELECT statement: "
    );

    sqlite3_bind_text(deviceStmt, 1, device.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(deviceStmt, 2, device.vendor.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(deviceStmt, 3, device.type.c_str(), -1, SQLITE_TRANSIENT);

    const int result = sqlite3_step(deviceStmt);

    if (result == SQLITE_DONE)
    {
        sqlite3_finalize(deviceStmt);
        return std::nullopt;
    }

    if (result != SQLITE_ROW)
    {
        const std::string error = sqlite3_errmsg(connection);
        sqlite3_finalize(deviceStmt);
        throw KttException("Failed to execute device SELECT statement: " + error);
    }

    const dbDeviceInfo output = dbDeviceInfo::FromRow(deviceStmt);
    sqlite3_finalize(deviceStmt);
    return output;
}

size_t DeviceRepository::CreateDeviceApi(sqlite3 *connection, const DeviceApi &deviceApi)
{
    const char *deviceApiSql = R"(
        INSERT INTO device_api
        (compute_api_id, version_major, version_minor, extensions)
        VALUES (?, ?, ?, ?)
    )";

    sqlite3_stmt *deviceApiStmt = DatabaseUtility::PrepareStatement(
        connection,
        deviceApiSql,
        "Failed to prepare device_api INSERT statement: "
    );

    sqlite3_bind_int(deviceApiStmt, 1, static_cast<int>(deviceApi.computeApi));
    DatabaseUtility::BindOptionalInt(deviceApiStmt, 2, deviceApi.cudaComputeCapabilityMajor);
    DatabaseUtility::BindOptionalInt(deviceApiStmt, 3, deviceApi.cudaComputeCapabilityMinor);
    DatabaseUtility::BindOptionalText(deviceApiStmt, 4, deviceApi.extensions);

    const int result = sqlite3_step(deviceApiStmt);

    if (result != SQLITE_DONE)
    {
        const std::string error = sqlite3_errmsg(connection);
        sqlite3_finalize(deviceApiStmt);
        throw KttException("Failed to execute device_api INSERT statement: " + error);
    }

    sqlite3_finalize(deviceApiStmt);
    return sqlite3_last_insert_rowid(connection);
}

std::optional<DeviceApi> DeviceRepository::GetDeviceApi(sqlite3 *connection, const DeviceApi &deviceApi)
{
    const char *deviceApiSql = R"(
        SELECT id, compute_api_id, version_major, version_minor, extensions
        FROM device_api
        WHERE compute_api_id = ? AND version_major = ? AND version_minor = ? AND extensions IS ?
        LIMIT 1
    )";

    sqlite3_stmt *deviceApiStmt = DatabaseUtility::PrepareStatement(
        connection,
        deviceApiSql,
        "Failed to prepare device_api SELECT statement: "
    );

    sqlite3_bind_int(deviceApiStmt, 1, static_cast<int>(deviceApi.computeApi));
    DatabaseUtility::BindOptionalInt(deviceApiStmt, 2, deviceApi.cudaComputeCapabilityMajor);
    DatabaseUtility::BindOptionalInt(deviceApiStmt, 3, deviceApi.cudaComputeCapabilityMinor);
    DatabaseUtility::BindOptionalText(deviceApiStmt, 4, deviceApi.extensions);

    const int result = sqlite3_step(deviceApiStmt);

    if (result == SQLITE_DONE)
    {
        sqlite3_finalize(deviceApiStmt);
        return std::nullopt;
    }

    if (result != SQLITE_ROW)
    {
        const std::string error = sqlite3_errmsg(connection);
        sqlite3_finalize(deviceApiStmt);
        throw KttException("Failed to execute device_api SELECT statement: " + error);
    }

    const DeviceApi output = DeviceApi::FromRow(deviceApiStmt);
    sqlite3_finalize(deviceApiStmt);
    return output;
}

Device DeviceRepository::GetOrCreateDevice(sqlite3 *connection, const Device &device)
{
    Device output = device;

    const dbDeviceInfo deviceInfo{device.id, device.name, device.vendor, device.type};
    if (auto existingDevice = GetDeviceInfo(connection, deviceInfo))
        output.id = existingDevice->id;
    else
        output.id = CreateDevice(connection, deviceInfo);

    const DeviceApi deviceApi{
        device.apiId,
        device.computeApi,
        device.extensions,
        device.cudaComputeCapabilityMajor,
        device.cudaComputeCapabilityMinor
    };

    if (auto existingDeviceApi = GetDeviceApi(connection, deviceApi))
        output.apiId = existingDeviceApi->id;
    else
        output.apiId = CreateDeviceApi(connection, deviceApi);

    return output;
}

dbDeviceInfo dbDeviceInfo::FromRow(sqlite3_stmt *stmt)
{
    return dbDeviceInfo{
        sqlite3_column_int64(stmt, 0),
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1)),
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2)),
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3))
    };
}

DeviceApi DeviceApi::FromRow(sqlite3_stmt *stmt)
{
    const bool extensionsIsNull = sqlite3_column_type(stmt, 4) == SQLITE_NULL;
    const bool majorIsNull = sqlite3_column_type(stmt, 2) == SQLITE_NULL;
    const bool minorIsNull = sqlite3_column_type(stmt, 3) == SQLITE_NULL;

    return DeviceApi{
        sqlite3_column_int64(stmt, 0),
        static_cast<ComputeApi>(sqlite3_column_int(stmt, 1)),
        extensionsIsNull ? std::nullopt : std::optional<std::string>(DatabaseUtility::ReadTextColumn(stmt, 4)),
        majorIsNull ? std::nullopt : std::optional<int>(sqlite3_column_int(stmt, 2)),
        minorIsNull ? std::nullopt : std::optional<int>(sqlite3_column_int(stmt, 3))
    };
}


std::optional<DeviceApi> DeviceRepository::GetDeviceApiBySimpleQuery(sqlite3 *connection, const DeviceApi &deviceApi)
{
    const char *deviceApiSql = R"(
        SELECT id, compute_api_id, version_major, version_minor, extensions
        FROM device_api
                WHERE compute_api_id = ?
                    AND version_major IS ?
                    AND version_minor IS ?
                    AND extensions IS ?
        LIMIT 1
    )";

    sqlite3_stmt *deviceApiStmt = DatabaseUtility::PrepareStatement(
        connection,
        deviceApiSql,
        "Failed to prepare device_api SELECT statement: "
    );

    sqlite3_bind_int(deviceApiStmt, 1, static_cast<int>(deviceApi.computeApi));
    DatabaseUtility::BindOptionalInt(deviceApiStmt, 2, deviceApi.cudaComputeCapabilityMajor);
    DatabaseUtility::BindOptionalInt(deviceApiStmt, 3, deviceApi.cudaComputeCapabilityMinor);
    DatabaseUtility::BindOptionalText(deviceApiStmt, 4, deviceApi.extensions);

    const int result = sqlite3_step(deviceApiStmt);

    if (result == SQLITE_DONE)
    {
        sqlite3_finalize(deviceApiStmt);
        return std::nullopt;
    }

    if (result != SQLITE_ROW)
    {
        const std::string error = sqlite3_errmsg(connection);
        sqlite3_finalize(deviceApiStmt);
        throw KttException("Failed to execute device_api SELECT statement: " + error);
    }

    const DeviceApi output = DeviceApi::FromRow(deviceApiStmt);
    sqlite3_finalize(deviceApiStmt);
    return output;
}

} // namespace ktt::db
