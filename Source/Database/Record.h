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
    std::filesystem::path m_KernelSourcePath;
    json m_BestResult;
};
}

