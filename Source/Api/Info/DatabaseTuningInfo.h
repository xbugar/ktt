#pragma once
#include <cstddef>
#include <optional>
#include <string>

#include <ComputeEngine/ComputeApi.h>

namespace ktt
{

struct DatabaseDeviceInfo
{
    std::string Name;
    std::string Vendor;
    std::string Type;
    std::optional<std::string> Extensions;
    std::optional<uint32_t> cudaComputeCapabilityMajor{};
    std::optional<uint32_t> cudaComputeCapabilityMinor{};
};

/** @struct DatabaseTuningInfo
 * Data transfer object which holds all information about tuning source and tuning space for which the results were
 * saved to database.
 */
struct DatabaseTuningInfo
{
    size_t parameterFingerprint{};
    size_t sourceFingerprint{};
    size_t spaceFingerprint{};
    ComputeApi computeApi{ComputeApi::Cpp};
    DatabaseDeviceInfo device{};
    std::optional<std::string> inputData{};
};

} // namespace ktt
