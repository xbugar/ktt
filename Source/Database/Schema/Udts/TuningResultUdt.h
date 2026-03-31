#pragma once
#include <cstddef>
#include <string>

#include <Output/JsonConverters.h>

namespace ktt
{

struct TuningResultSaveUdt
{
    std::size_t spaceId;
    std::size_t duration;
    std::string runId;
    json result;
};

struct TuningResultLoadUdt
{
    std::size_t id{};
    std::string runId;
    std::size_t spaceId{};
    std::size_t duration{};
    json result;
};
} // namespace ktt
