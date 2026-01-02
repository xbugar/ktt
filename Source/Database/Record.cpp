#include <Database/Record.h>

#include <utility>

namespace ktt
{

constexpr char delim = '|';

Record::Record(const std::size_t finger, std::filesystem::path path, json result) :
        m_ParameterFingerprint(finger),
        m_KernelSourcePath(std::move(path)),
        m_BestResult(std::move(result)) {}

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
    r.m_BestResult = json::parse(cell);

    return r;
}

std::string Record::Serialize() const
{
    std::stringstream ss;
    ss << m_ParameterFingerprint << delim << m_KernelSourcePath << delim << m_BestResult.dump();
    return ss.str();
}
} // namespace ktt
