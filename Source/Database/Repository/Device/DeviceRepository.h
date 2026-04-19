#pragma once

#include <cstddef>
#include <sqlite3.h>
#include <string>

#include <ComputeEngine/ComputeApi.h>

namespace ktt
{

struct DeviceDescriptor
{
    std::string name;
    std::string vendor;
    std::string type;
    std::string extensions;
    int cudaComputeCapabilityMajor;
    int cudaComputeCapabilityMinor;
};

class DeviceRepository
{
public:
    static size_t GetOrCreateDevice(sqlite3* connection, ComputeApi computeApi, const DeviceDescriptor& device);
};

} // namespace ktt

