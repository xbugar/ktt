#pragma once

#include <string>
#include <vector>
#include <optional>

#include <Database/Schema/Udts/TuningSpaceUdt.h>

namespace ktt
{

struct TuningSourceSaveUdt
{
    size_t parameterFingerprint;
    size_t sourceFingerprint;
    std::string* source;
};

struct TuningSourceDto
{
    size_t id;
    size_t parameterFingerprint;
    size_t sourceFingerprint;
    std::string createdAt;
    std::optional<std::vector<TuningSpaceLoadUdt>> spaces;
};

} // namespace ktt
