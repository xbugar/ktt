#pragma once

#include <Ktt.h>
#include <cstddef>
#include <optional>
#include <sqlite3.h>
#include <string>


namespace ktt::db
{

struct DbDeviceInfo
{
    std::optional<size_t> id;
    std::string name;
    std::string vendor;
    std::string type;

    static DbDeviceInfo FromRow(sqlite3_stmt *stmt);
};

struct DeviceApi
{
    std::optional<size_t> id;
    ComputeApi computeApi;
    std::optional<std::string> extensions;
    std::optional<int> cudaComputeCapabilityMajor;
    std::optional<int> cudaComputeCapabilityMinor;

    static DeviceApi FromRow(sqlite3_stmt *stmt);
};

struct Device
{
    std::optional<size_t> id;
    std::optional<size_t> apiId;
    std::string name;
    std::string vendor;
    std::string type;
    ComputeApi computeApi;
    std::optional<std::string> extensions;
    std::optional<int> cudaComputeCapabilityMajor;
    std::optional<int> cudaComputeCapabilityMinor;
};

class DeviceRepository
{
public:
    static Device GetOrCreateDevice(sqlite3 *connection, const Device &device);
    static size_t CreateDevice(sqlite3 *connection, const DbDeviceInfo &device);
    static size_t CreateDeviceApi(sqlite3 *connection, const DeviceApi &deviceApi);
    static std::optional<DbDeviceInfo> GetDeviceInfo(sqlite3 *connection, const DbDeviceInfo &device);
    static std::optional<DeviceApi> GetDeviceApi(sqlite3 *connection, const DeviceApi &deviceApi);
    static std::optional<DeviceApi> GetDeviceApiForSimpleQuery(sqlite3 *connection, const DeviceApi &deviceApi);
};

} // namespace ktt::db
