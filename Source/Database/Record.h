#pragma once

#include <Output/JsonConverters.h>

namespace ktt
{
struct Record
{
    Record() = default;

    std::size_t m_Id{};
    std::size_t m_ParameterFingerprint{};
    std::size_t m_SourceFingerprint{};
    std::size_t m_TuningSpaceFingerprint{};
    std::string m_Gpu;
    json m_BestResult;
};
}

