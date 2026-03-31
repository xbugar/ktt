#pragma once
#include <cstddef>
#include <string>

namespace ktt
{

struct TuningRunSaveUdt
{
    std::string uuid;
};

struct TuningRunLoadUdt
{
    std::size_t id{};
    std::string uuid;
    std::string createdAt;
};
} // namespace ktt
