#pragma once
#include <cstddef>
#include <optional>
#include <string>

#include <ComputeEngine/ComputeApi.h>

namespace ktt::db
{

struct DeviceInfo
{
    std::string Name;
    std::string Vendor;
    std::string Type;
    std::optional<std::string> Extensions;
    std::optional<uint32_t> cudaComputeCapabilityMajor{};
    std::optional<uint32_t> cudaComputeCapabilityMinor{};
    ComputeApi computeApi{ComputeApi::Cpp};
};

struct TuningSpaceInfo
{
    size_t parameterFingerprint{};
    size_t sourceFingerprint{};
    size_t spaceFingerprint{};
};

/** @struct TuningInfo
 * Data transfer object which holds all information about tuning source and tuning space for which the results were
 * saved to database.
 */
struct TuningInfo
{
    DeviceInfo device{};
    std::optional<std::string> inputData{};
    TuningSpaceInfo spaceInfo{};
};

} // namespace ktt::db
