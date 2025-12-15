//
// Created by andre on 12/15/2025.
//

#include "Record.h"

namespace ktt
{

constexpr char delim = '|';

Record Record::Deserialize(const std::string& line)
{
    std::stringstream ss(line);
    std::string cell;
    Record r;

    std::getline(ss, cell, delim);
    r.m_ParameterFingerprint = std::stoull(cell);

    std::getline(ss, cell, delim);
    r.m_KernelSourcePath = cell;

    std::getline(ss, cell, delim);
    r.m_BestResult = cell;

    return r;
}

std::string Record::Serialize() const
{
    std::stringstream ss;
    ss << m_ParameterFingerprint << delim << m_KernelSourcePath << delim << m_BestResult.dump();
    return ss.str();
}
}
