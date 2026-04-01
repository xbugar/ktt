#pragma once
#include <cstddef>
#include <string>

#include <Output/JsonConverters.h>

#include <Database/Schema/Udts/TuningSpaceUdt.h>
#include <Database/Schema/Udts/TuningRunUdt.h>

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
    std::size_t duration{};
    json result;

    TuningSpaceLoadUdt tuningSpace;
    TuningRunLoadUdt tuningRun;
};
} // namespace ktt
