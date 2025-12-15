#pragma once

#include <string>
#include <JsonCommandConverters.h>
#include <filesystem>

namespace ktt
{
struct Record
{
    static Record Deserialize(const std::string& line);

    [[nodiscard]] std::string Serialize() const;

    std::size_t m_ParameterFingerprint;
    std::filesystem::path m_KernelSourcePath;
    json m_BestResult;
};
}

