#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include <Database/Schema/Udts/TuningSpaceUdt.h>

namespace ktt
{

struct TuningSourceSaveUdt
{
    std::size_t parameterFingerprint;
    std::size_t sourceFingerprint;
};

struct TuningSourceLoadUdt
{
    std::size_t id{};
    std::size_t parameterFingerprint{};
    std::size_t sourceFingerprint{};
    std::string createdAt;
    std::vector<TuningSpaceLoadUdt> spaces;
};

} // namespace ktt
