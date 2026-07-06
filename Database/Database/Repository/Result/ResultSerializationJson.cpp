#include <Database/Repository/Result/ResultSerialization.h>

#include <json.hpp>

#include <Output/JsonConverters.h>

namespace ktt::db
{

std::string SerializeResultJson(const KernelResult &result, const int indent)
{
    return nlohmann::json(result).dump(indent);
}

KernelResult DeserializeResultJson(const std::string &text)
{
    return nlohmann::json::parse(text).get<KernelResult>();
}

} // namespace ktt::db
