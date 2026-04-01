#pragma once
#include <cstddef>
#include <string>

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
};

} // namespace ktt

