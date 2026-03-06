#pragma once

#include <Output/JsonConverters.h>

namespace ktt
{
struct Record
{
    Record() = default;

    std::size_t m_ParameterFingerprint{};
    std::size_t m_SourceFingerprint{};
    std::size_t m_ConstraintFingerprint{};
    bool m_HasUndefinedConstraintLambda{};
    json m_BestResult;
};
}

