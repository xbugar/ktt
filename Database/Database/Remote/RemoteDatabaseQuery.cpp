#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include <json.hpp>

#include <Api/KttException.h>
#include <Database/Remote/RemoteClient.h>
#include <Database/Remote/RemoteDatabase.h>
#include <Database/Remote/RemoteDatabaseShared.h>
#include <Database/Remote/WireFormat.h>
#include <Database/Repository/Utility.h>
#include <Database/Utility/TimeUnitGuard.h>
#include <Database/Utility/Uuid.h>

namespace ktt::db
{

namespace
{

using json = nlohmann::json;

// The server's limits for $top and $skip (query.md, "Limits and safety").
constexpr uint64_t MaxTop = 1000;
constexpr uint64_t MaxSkip = 100000;

// The groups QueryResults reads besides the core fields: run (id, input data, and the format and time unit of the
// payload), device and source. Never tuner or space, which ResultQueryItem does not hold.
const std::string SelectGroups = "run,device,source";

// The body of POST /result/query (ResultQueryRequest). The options have no "$", and count and distinct are not sent.
json QueryToWire(const ResultQuery &query)
{
    return json{
        {"filter", query.filter},
        {"orderby", query.orderBy},
        {"top", query.top},
        {"skip", query.skip},
        {"select", query.includeResult ? SelectGroups + ",result" : SelectGroups}};
}

uint64_t DurationFromWire(const json &item, const char *key)
{
    const json &value = item.at(key);

    // The parser stores every non-negative integer as unsigned, so anything else is negative or not an integer.
    if (!value.is_number_unsigned())
        throw KttException(std::string("\"") + key + "\" in the response is not a non-negative integer");

    return value.get<uint64_t>();
}

ParameterValue ParameterValueFromWire(const std::string &name, const json &value)
{
    if (value.is_boolean())
        return value.get<bool>();
    if (value.is_number_unsigned())
        return value.get<uint64_t>();
    if (value.is_number_integer())
        return value.get<int64_t>();
    if (value.is_number_float())
        return value.get<double>();
    if (value.is_string())
        return value.get<std::string>();

    throw KttException("the value of configuration parameter '" + name
        + "' in the response is not a number, boolean or string");
}

std::map<std::string, ParameterValue> ConfigurationFromWire(const json &configuration)
{
    if (!configuration.is_object())
        throw KttException("\"configuration\" in the response is not an object");

    std::map<std::string, ParameterValue> parameters;

    for (const auto &[name, value] : configuration.items())
        parameters.emplace(name, ParameterValueFromWire(name, value));

    return parameters;
}

ResultQueryItem ItemFromWire(const json &item, const bool includeResult)
{
    const json &run = item.at("run");

    ResultQueryItem result;
    result.duration = DurationFromWire(item, "duration");
    result.totalDuration = DurationFromWire(item, "total_duration");
    result.configuration = ConfigurationFromWire(item.at("configuration"));
    result.runId = UuidGenerator::ToString(UuidGenerator::Parse(run.at("id").get<std::string>()));

    const auto input = run.find("input_data");

    if (input != run.end() && !input->is_null())
        result.inputData = input->get<std::string>();

    result.device = remote::ReadDevice(item.at("device"));
    result.sourceFingerprint =
        FingerprintToWire(FingerprintFromWire(item.at("source").at("fingerprint").get<std::string>()));

    if (includeResult)
    {
        // The payload's durations are in the unit of its run, which can differ from the unit active now.
        const auto format = remote::ReadOutputFormat(run.at("output_format").get<std::string>());
        const auto timeUnit = TimeUnitFromName(run.at("time_unit").get<std::string>());
        const json &payload = item.at("result");

        if (!payload.is_object())
            throw KttException("a result payload in the response is not a JSON object");

        result.result = DatabaseUtility::DeserializeResult(payload.dump(), format, timeUnit);
    }

    return result;
}

} // namespace

std::vector<ResultQueryItem> RemoteDatabase::QueryResults(const ResultQuery &query) const
{
    static const std::string Operation = "RemoteDatabase::QueryResults";

    // As $top=0 in OData, and as a limit of 0 in the other reads.
    if (query.top == 0)
        return {};

    if (query.top > MaxTop || query.skip > MaxSkip)
    {
        throw KttException(Operation + ": top must be at most " + std::to_string(MaxTop) + " and skip at most "
            + std::to_string(MaxSkip) + ", got top " + std::to_string(query.top) + " and skip "
            + std::to_string(query.skip));
    }

    std::vector<ResultQueryItem> items;

    remote::RunTranslated(Operation, *m_Client, [&]
    {
        const json body = QueryToWire(query);
        // The endpoint has no "not found": an unknown source or space in the filter matches nothing (200).
        const auto response = remote::CallServer(*m_Client, Operation, "POST", "/result/query", &body, false);
        const json &wireItems = response->at("items");

        if (!wireItems.is_array())
            throw KttException("\"items\" in the response is not an array");

        items.reserve(wireItems.size());

        for (const auto &item : wireItems)
            items.push_back(ItemFromWire(item, query.includeResult));
    });

    return items;
}

} // namespace ktt::db
