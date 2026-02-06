#pragma once

#include <string>
#include <Output/JsonConverters.h>
#include <filesystem>

namespace ktt
{
struct Record
{
    Record() = default;
    Record(std::size_t finger, std::filesystem::path  path, json  result);
    static Record Deserialize(const std::string& line);

    std::string Serialize() const;

    std::size_t m_ParameterFingerprint{};
    std::size_t m_SourceFingerprint{};
    std::size_t m_ConstraintFingerprint{};
    json m_BestResult;
};
}

