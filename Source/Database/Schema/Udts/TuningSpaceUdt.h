#pragma once
#include <cstddef>
#include <string>
#include <vector>

#include <Database/Schema/Udts/TuningResultUdt.h>

namespace ktt
{
struct TuningSpaceSaveUdt
{
    std::size_t sourceId;
    std::size_t spaceFingerprint;
};

struct TuningSpaceLoadUdt
{
    std::size_t id{};
    std::size_t sourceId{};
    std::size_t spaceFingerprint{};
    std::string createdAt;
    std::vector<TuningResultLoadUdt> results;
};

} // namespace ktt

