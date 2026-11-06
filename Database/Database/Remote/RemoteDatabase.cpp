#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iterator>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <utility>

#include <json.hpp>

#include <Api/KttException.h>
#include <Database/Remote/Http/CurlHttpClient.h>
#include <Database/Remote/RemoteClient.h>
#include <Database/Remote/RemoteDatabase.h>
#include <Database/Remote/WireFormat.h>
#include <Database/Repository/Utility.h>
#include <Database/Utility/TimeUnitGuard.h>
#include <Database/Utility/TuningInfoValidation.h>
#include <Database/Utility/Uuid.h>
#include <Output/TimeConfiguration/TimeConfiguration.h>
#include <Utility/Logger/Logger.h>

namespace ktt::db
{

namespace
{

using json = nlohmann::json;

// The server's maximum for the result limits of POST /result/best and POST .../space/{id}/result.
constexpr uint32_t MaxResultLimit = 1000;

// Page size of GET .../space/{id}/run: the server's maximum, and the batch size of the local Database.
constexpr size_t RunPageSize = 500;

// Longest part of a response body quoted in an exception message.
constexpr size_t MaxQuotedBodyLength = 300;

// A result read from the server, with the kernel duration in nanoseconds the server orders by (ResultOut.duration).
struct RemoteResult
{
    int64_t duration;
    KernelResult result;
};

std::string ComputeApiToWire(const ComputeApi api)
{
    switch (api)
    {
    case ComputeApi::OpenCL:
        return "opencl";
    case ComputeApi::CUDA:
        return "cuda";
    case ComputeApi::Vulkan:
        return "vulkan";
    case ComputeApi::Cpp:
        return "cpp";
    }

    throw KttException("unknown compute API value " + std::to_string(static_cast<int>(api)));
}

ComputeApi ComputeApiFromWire(const std::string &name)
{
    for (const auto api : {ComputeApi::OpenCL, ComputeApi::CUDA, ComputeApi::Vulkan, ComputeApi::Cpp})
    {
        if (ComputeApiToWire(api) == name)
            return api;
    }

    throw KttException("unknown compute API '" + name + "' in the response");
}

// The server stores no XML, so a client configured for XML sends JSON (cpp-requirements.md R4.1.3).
OutputFormat GetWireFormat(const OutputFormat format)
{
    if (format != OutputFormat::XML)
        return format;

    static std::once_flag logged;
    std::call_once(logged, []
    {
        Logger::LogWarning("The remote database stores no XML; results saved with OutputFormat::XML are stored as JSON");
    });

    return OutputFormat::JSON;
}

std::string OutputFormatToWire(const OutputFormat format)
{
    return format == OutputFormat::JSON_T4 ? "json_t4" : "json";
}

OutputFormat OutputFormatFromWire(const std::string &name)
{
    if (name == "json")
        return OutputFormat::JSON;
    if (name == "json_t4")
        return OutputFormat::JSON_T4;

    throw KttException("unknown output format '" + name + "' in the response");
}

json OptionalToWire(const std::optional<std::string> &value)
{
    return value.has_value() ? json(*value) : json(nullptr);
}

json OptionalToWire(const std::optional<uint32_t> &value)
{
    return value.has_value() ? json(*value) : json(nullptr);
}

json DeviceToWire(const DeviceInfo &device)
{
    return json{
        {"name", device.name},
        {"vendor", device.vendor},
        {"type", device.type},
        {"compute_api", ComputeApiToWire(device.computeApi)},
        {"device_identifier", OptionalToWire(device.deviceIdentifier)},
        // The local Database stores an unknown driver as ""; the server's "unknown" is null.
        {"driver_version", device.driverVersion.empty() ? json(nullptr) : json(device.driverVersion)},
        {"extensions", OptionalToWire(device.extensions)},
        {"cuda_compute_capability_major", OptionalToWire(device.cudaComputeCapabilityMajor)},
        {"cuda_compute_capability_minor", OptionalToWire(device.cudaComputeCapabilityMinor)}};
}

// Reads a device the way the local RunRepository does: an empty identifier or extensions string is no value, and an
// unknown driver version is "".
DeviceInfo DeviceFromWire(const json &device)
{
    const auto optionalText = [&device](const char *key) -> std::optional<std::string>
    {
        const auto it = device.find(key);

        if (it == device.end() || it->is_null())
            return std::nullopt;

        auto text = it->get<std::string>();
        return text.empty() ? std::nullopt : std::optional<std::string>(std::move(text));
    };

    const auto optionalNumber = [&device](const char *key) -> std::optional<uint32_t>
    {
        const auto it = device.find(key);

        if (it == device.end() || it->is_null())
            return std::nullopt;

        return it->get<uint32_t>();
    };

    DeviceInfo info;
    info.name = device.at("name").get<std::string>();
    info.vendor = device.at("vendor").get<std::string>();
    info.type = device.at("type").get<std::string>();
    info.computeApi = ComputeApiFromWire(device.at("compute_api").get<std::string>());
    info.deviceIdentifier = optionalText("device_identifier");
    info.driverVersion = optionalText("driver_version").value_or("");
    info.extensions = optionalText("extensions");
    info.cudaComputeCapabilityMajor = optionalNumber("cuda_compute_capability_major");
    info.cudaComputeCapabilityMinor = optionalNumber("cuda_compute_capability_minor");
    return info;
}

json TuningInfoToWire(const TuningInfo &info)
{
    return json{
        {"device", DeviceToWire(info.device)},
        {"tuner", {{"name", info.tuner.name}, {"version", info.tuner.version}}},
        {"input_data", OptionalToWire(info.inputData)},
        {"space_info",
            {{"source_fingerprint", FingerprintToWire(info.spaceInfo.sourceFingerprint)},
                {"parameter_fingerprint", FingerprintToWire(info.spaceInfo.parameterFingerprint)},
                {"space_fingerprint", FingerprintToWire(info.spaceInfo.spaceFingerprint)},
                {"source_name", OptionalToWire(info.spaceInfo.sourceName)},
                {"space_name", OptionalToWire(info.spaceInfo.spaceName)}}}};
}

json ParameterValueToWire(const ParameterPair &pair)
{
    const ParameterValue &value = pair.GetValue();

    switch (pair.GetValueType())
    {
    case ParameterValueType::Int:
        return std::get<int64_t>(value);
    case ParameterValueType::UnsignedInt:
        return std::get<uint64_t>(value);
    case ParameterValueType::Double:
        return std::get<double>(value);
    case ParameterValueType::Bool:
        return std::get<bool>(value);
    case ParameterValueType::String:
    case ParameterValueType::CompilerParameter:
    default:
        return pair.GetValueString();
    }
}

json ResultToWire(const KernelResult &result, const OutputFormat format)
{
    json configuration = json::object();

    for (const auto &pair : result.GetConfiguration().GetPairs())
        configuration[pair.GetName()] = ParameterValueToWire(pair);

    return json{
        {"duration", result.GetKernelDuration()},
        {"total_duration", result.GetTotalDuration()},
        {"status", "ok"},
        {"configuration", std::move(configuration)},
        {"result", json::parse(DatabaseUtility::SerializeResult(result, format, 0))}};
}

// Reads a ResultList, in the server's order. The durations inside each payload are read in the unit of its run.
std::vector<RemoteResult> ResultsFromWire(const json &body)
{
    const json &items = body.at("results");

    if (!items.is_array())
        throw KttException("\"results\" in the response is not an array");

    std::vector<RemoteResult> results;
    results.reserve(items.size());

    for (const auto &item : items)
    {
        const auto format = OutputFormatFromWire(item.at("output_format").get<std::string>());
        const auto timeUnit = TimeUnitFromName(item.at("time_unit").get<std::string>());
        const json &payload = item.at("result");

        if (!payload.is_object())
            throw KttException("a result payload in the response is not a JSON object");

        results.push_back({item.at("duration").get<int64_t>(),
            DatabaseUtility::DeserializeResult(payload.dump(), format, timeUnit)});
    }

    return results;
}

std::vector<KernelResult> ToKernelResults(std::vector<RemoteResult> results)
{
    std::vector<KernelResult> kernelResults;
    kernelResults.reserve(results.size());

    for (auto &result : results)
        kernelResults.push_back(std::move(result.result));

    return kernelResults;
}

size_t CountFromWire(const json &body, const char *key)
{
    const auto count = body.at(key).get<int64_t>();

    if (count < 0)
        throw KttException(std::string("\"") + key + "\" in the response is negative");

    return static_cast<size_t>(count);
}

std::string Quote(const std::string &text)
{
    if (text.size() <= MaxQuotedBodyLength)
        return text;

    return text.substr(0, MaxQuotedBodyLength) + "...";
}

// Whether a body is a ktt-db error body: a JSON object with a string "error" (endpoints.md, "Errors").
bool IsErrorBody(const std::string &body)
{
    const json parsed = json::parse(body, nullptr, false);
    return parsed.is_object() && parsed.contains("error") && parsed["error"].is_string();
}

// The server's explanation of an error response: the "error" (and "code") of an ApiErrorBody, or the start of the
// body when it is something else, e.g. the HTML page of a proxy.
std::string DescribeErrorBody(const std::string &body)
{
    const json parsed = json::parse(body, nullptr, false);

    if (parsed.is_object() && parsed.contains("error") && parsed["error"].is_string())
    {
        std::string text = parsed["error"].get<std::string>();

        if (parsed.contains("code") && parsed["code"].is_string())
            text += " (code " + parsed["code"].get<std::string>() + ")";

        return Quote(text);
    }

    if (body.empty())
        return "empty response body";

    return "response body is not a ktt-db error: " + Quote(body);
}

// Sends one request and returns its JSON body. Every request may be retried (RetryPolicy::Idempotent): the reads are
// safe to repeat, and POST /run carries the run id, so the server recognizes a repeated save.
// @return The body of a 200 or 201 response; std::nullopt for 404 when notFoundIsEmpty and the body is a ktt-db error
//         body. Any other 404 (an unmatched route has an empty body, a proxy sends its own page) throws, so a wrong base
//         URL is not mistaken for "not found".
// @throw KttException For every other status, a network error, or a success body that is not a JSON object. The
//        message has the operation, the request and the status, never the API key.
std::optional<json> Call(const RemoteClient &client, const std::string &operation, const std::string_view method,
    const std::string &path, const json *body, const bool notFoundIsEmpty)
{
    HttpResponse response;

    try
    {
        response = client.Send(method, path, body != nullptr ? body->dump() : std::string(), RetryPolicy::Idempotent);
    }
    catch (const KttException &error)
    {
        throw KttException(operation + " failed: " + client.Redact(error.what()));
    }

    if (response.status == 404 && notFoundIsEmpty && IsErrorBody(response.body))
        return std::nullopt;

    const std::string request = std::string(method) + " " + path;

    if (response.status != 200 && response.status != 201)
    {
        throw KttException(operation + " failed: HTTP " + std::to_string(response.status) + " from " + request + ": "
            + DescribeErrorBody(client.Redact(response.body)));
    }

    json parsed = json::parse(response.body, nullptr, false);

    if (!parsed.is_object())
    {
        throw KttException(operation + " failed: the body of HTTP " + std::to_string(response.status) + " from "
            + request + " is not a JSON object: " + Quote(client.Redact(response.body)));
    }

    return parsed;
}

// Carries an exception thrown by a user's predicate to Translate, which rethrows the original exception unchanged.
struct PredicateError
{
    std::exception_ptr error;
};

template <typename Predicate, typename Argument>
bool CallPredicate(const Predicate &predicate, const Argument &argument)
{
    try
    {
        return predicate(argument);
    }
    catch (...)
    {
        throw PredicateError{std::current_exception()};
    }
}

// Runs the body of a public method so that every error names the method: a JSON error (a missing field or a wrong
// type in a response, or request text that is not valid UTF-8) and a KttException from the conversions become a
// KttException that starts with the operation. Other exceptions pass unchanged; a user's predicate is called through
// CallPredicate, so its exceptions, a KttException included, reach the caller unchanged.
template <typename Function>
auto Translate(const std::string &operation, const RemoteClient &client, Function function) -> decltype(function())
{
    try
    {
        return function();
    }
    catch (const PredicateError &error)
    {
        std::rethrow_exception(error.error);
    }
    catch (const json::exception &error)
    {
        throw KttException(operation + " failed: invalid JSON: " + client.Redact(error.what()));
    }
    catch (const KttException &error)
    {
        const std::string message = error.what();

        if (message.rfind(operation, 0) == 0)
            throw;

        throw KttException(operation + " failed: " + client.Redact(message), error.GetReason());
    }
}

void CheckLimit(const std::string &operation, const uint32_t limit)
{
    if (limit > MaxResultLimit)
    {
        throw KttException(operation + ": the limit must be at most " + std::to_string(MaxResultLimit)
            + " for a remote database, got " + std::to_string(limit));
    }
}

} // namespace

RemoteDatabase::RemoteDatabase(std::string baseUrl, std::string apiKey, RemoteDatabaseOptions options) :
    RemoteDatabase(std::move(baseUrl), std::move(apiKey), options, std::make_shared<CurlHttpClient>(options))
{}

RemoteDatabase::RemoteDatabase(std::string baseUrl, std::string apiKey, RemoteDatabaseOptions options,
    std::shared_ptr<HttpClient> httpClient) :
    m_Client(std::make_unique<RemoteClient>(std::move(baseUrl), std::move(apiKey), options.maxRetries,
        std::move(httpClient)))
{}

RemoteDatabase::~RemoteDatabase() = default;

void RemoteDatabase::SaveResults(const TuningInfo &source, std::vector<KernelResult> results,
    const SaveOptions options) const
{
    static const std::string Operation = "RemoteDatabase::SaveResults";
    ValidateTuningInfo(source);

    Translate(Operation, *m_Client, [&]
    {
        // Generated once, before the request: every retry sends the same id, so the server saves the run only once.
        const uuid runId = UuidGenerator::GenerateUuidV7();
        const OutputFormat format = GetWireFormat(options.format);
        json body;

        {
            // The payloads are written in the current time unit, which time_unit records. The lock keeps other
            // database threads from switching the unit while the results are serialized.
            const auto lock = LockTimeUnit();
            json wireResults = json::array();

            for (const auto &result : results)
            {
                if (result.GetStatus() == ResultStatus::Ok)
                    wireResults.push_back(ResultToWire(result, format));
            }

            body = json{
                {"id", UuidGenerator::ToString(runId)},
                {"tuning_info", TuningInfoToWire(source)},
                {"output_format", OutputFormatToWire(format)},
                {"time_unit", TimeUnitToName(TimeConfiguration::GetInstance().GetTimeUnit())},
                {"results", std::move(wireResults)}};
        }

        Call(*m_Client, Operation, "POST", "/run", &body, false);
    });
}

std::vector<KernelResult> RemoteDatabase::SimpleGetBestResults(const TuningInfo &source, const uint32_t limit) const
{
    static const std::string Operation = "RemoteDatabase::SimpleGetBestResults";
    ValidateTuningInfo(source);

    if (limit == 0)
        return {};

    CheckLimit(Operation, limit);

    return Translate(Operation, *m_Client, [&]() -> std::vector<KernelResult>
    {
        const json body{{"tuning_info", TuningInfoToWire(source)}, {"limit", limit}};
        // The endpoint answers an unknown source or space with an empty list, so a 404 is an error here.
        const auto response = Call(*m_Client, Operation, "POST", "/result/best", &body, false);
        return ToKernelResults(ResultsFromWire(*response));
    });
}

std::vector<KernelResult> RemoteDatabase::GetBestResults(const GetBestResultsQuery &query) const
{
    static const std::string Operation = "RemoteDatabase::GetBestResults";

    if (query.limit == 0)
        return {};

    CheckLimit(Operation, query.limit);

    return Translate(Operation, *m_Client, [&]() -> std::vector<KernelResult>
    {
        const std::string sourcePath = "/source/" + FingerprintToWire(query.source.sourceFingerprint);
        const auto space = Call(*m_Client, Operation, "GET", sourcePath + "/space?parameter_fingerprint="
            + FingerprintToWire(query.source.parameterFingerprint) + "&space_fingerprint="
            + FingerprintToWire(query.source.spaceFingerprint), nullptr, true);

        if (!space)
            return {};

        const std::string spacePath = sourcePath + "/space/" + std::to_string(space->at("space_id").get<int64_t>());

        // Offset paging shifts when a run becomes visible meanwhile, so a run can come back on a later page.
        std::set<uuid> seenRuns;
        std::vector<RemoteResult> bestResults;

        for (size_t offset = 0;;)
        {
            const auto page = Call(*m_Client, Operation, "GET", spacePath + "/run?offset=" + std::to_string(offset)
                + "&limit=" + std::to_string(RunPageSize), nullptr, true);

            if (!page)
                return {};

            const json &runs = page->at("runs");

            if (!runs.is_array())
                throw KttException(Operation + " failed: \"runs\" in the response is not an array");

            json runIds = json::array();

            for (const auto &run : runs)
            {
                const uuid runId = UuidGenerator::Parse(run.at("run_id").get<std::string>());

                if (!seenRuns.insert(runId).second)
                    continue;

                bool keep = true;

                if (query.devicePredicate)
                    keep = CallPredicate(*query.devicePredicate, DeviceFromWire(run.at("device")));

                // As in the local Database: with an input predicate, a run without input data is left out.
                if (keep && query.inputPredicate)
                {
                    const auto input = run.find("input_data");
                    keep = input != run.end() && !input->is_null()
                        && CallPredicate(*query.inputPredicate, input->get<std::string>());
                }

                if (keep)
                    runIds.push_back(UuidGenerator::ToString(runId));
            }

            if (!runIds.empty())
            {
                const json body{{"run_ids", std::move(runIds)}, {"limit", query.limit}};
                const auto response = Call(*m_Client, Operation, "POST", spacePath + "/result", &body, true);

                if (!response)
                    return {};

                auto batch = ResultsFromWire(*response);

                // Both ranges are sorted by kernel duration: bestResults by earlier merges, the batch by the server.
                // The merge is stable, so equal durations keep their order and earlier pages win ties, as locally.
                const auto oldSize = static_cast<std::ptrdiff_t>(bestResults.size());
                bestResults.insert(bestResults.end(), std::make_move_iterator(batch.begin()),
                    std::make_move_iterator(batch.end()));
                std::inplace_merge(bestResults.begin(), bestResults.begin() + oldSize, bestResults.end(),
                    [](const RemoteResult &left, const RemoteResult &right) { return left.duration < right.duration; });

                if (bestResults.size() > query.limit)
                    bestResults.erase(bestResults.begin() + query.limit, bestResults.end());
            }

            if (runs.size() < RunPageSize)
                break;

            offset += runs.size();
        }

        return ToKernelResults(std::move(bestResults));
    });
}

std::optional<SourceStats> RemoteDatabase::GetStatsForSource(const size_t sourceFingerprint) const
{
    static const std::string Operation = "RemoteDatabase::GetStatsForSource";

    return Translate(Operation, *m_Client, [&]() -> std::optional<SourceStats>
    {
        const auto response = Call(*m_Client, Operation, "GET",
            "/source/" + FingerprintToWire(sourceFingerprint) + "/stats", nullptr, true);

        if (!response)
            return std::nullopt;

        SourceStats stats;
        stats.spaceCount = CountFromWire(*response, "space_count");
        stats.deviceCount = CountFromWire(*response, "device_count");
        stats.runCount = CountFromWire(*response, "run_count");
        stats.resultCount = CountFromWire(*response, "result_count");
        // The application API returns no names (cpp-requirements.md R1.4).
        stats.name = std::nullopt;
        return stats;
    });
}

} // namespace ktt::db
