#pragma once

// In-memory fake of the ktt-db application API (/ktt-db/app/...) for the remote database parity tests (R8.2). Included
// only inside the KTT_REMOTE_DATABASE guard. It plays the server behind an HttpClient: it keeps sources, spaces, runs
// and results in memory and answers the endpoints that RemoteDatabase uses, following the rules documented in the
// server's OpenAPI (ktt_db.json). One API key sees everything (a single realm). A request that breaks the documented
// shape gets 400, so a parity test also checks the request bodies. Thread-safe.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include <json.hpp>

#include <Database/Remote/Http/HttpClient.h>
#include <Database/Utility/Uuid.h>

namespace ktt::db::test
{

class FakeKttDbServer : public HttpClient
{
public:
    using json = nlohmann::json;

    explicit FakeKttDbServer(std::string baseUrl = "http://ktt-db.test") :
        m_Prefix(std::move(baseUrl) + "/ktt-db/app")
    {}

    HttpResponse Send(std::string_view method, const std::string &url, const HttpHeaders &headers,
        const std::string &body) override
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        ++m_RequestCount;

        if (!HasHeader(headers, "x-api-key") || !HasHeader(headers, "correlation-id"))
            return Error(400, "missing x-api-key or correlation-id");

        if (!body.empty() && !HasHeader(headers, "content-type"))
            return Error(400, "missing content-type");

        if (url.rfind(m_Prefix, 0) != 0)
            return Error(404, "not found");

        const std::string target = url.substr(m_Prefix.size());
        const size_t question = target.find('?');
        const std::string path = target.substr(0, question);
        const auto query = ParseQuery(question == std::string::npos ? std::string() : target.substr(question + 1));
        const auto segments = Split(path);

        try
        {
            const json request = body.empty() ? json() : json::parse(body);

            if (method == "POST" && path == "/run")
                return SaveRun(request);
            if (method == "POST" && path == "/result/best")
                return BestResults(request);
            if (segments.size() >= 3 && segments[0] == "source")
            {
                const std::string &fingerprint = segments[1];

                if (method == "GET" && segments.size() == 3 && segments[2] == "stats")
                    return Stats(fingerprint);
                if (method == "GET" && segments.size() == 3 && segments[2] == "space")
                    return GetSpace(fingerprint, query);
                if (segments.size() == 5 && segments[2] == "space")
                {
                    const int64_t spaceId = std::stoll(segments[3]);

                    if (method == "GET" && segments[4] == "run")
                        return SpaceRuns(fingerprint, spaceId, query);
                    if (method == "POST" && segments[4] == "result")
                        return RunResults(fingerprint, spaceId, request);
                }
            }

            return Error(404, "not found");
        }
        catch (const std::exception &error)
        {
            return Error(400, std::string("invalid request: ") + error.what());
        }
    }

    size_t RequestCount() const
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_RequestCount;
    }

    size_t RunCount() const
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_Runs.size();
    }

private:
    struct Space
    {
        int64_t id;
        std::string sourceFingerprint;
        std::string parameterFingerprint;
        std::string spaceFingerprint;
    };

    struct Result
    {
        int64_t id;
        int64_t duration;
        int64_t totalDuration;
        json payload;
    };

    struct Run
    {
        std::string id;
        int64_t spaceId;
        json device;
        json tuner;
        json inputData;
        std::string outputFormat;
        std::string timeUnit;
        std::vector<Result> results;
    };

    std::string m_Prefix;
    mutable std::mutex m_Mutex;
    size_t m_RequestCount = 0;
    std::map<std::string, int64_t> m_Sources; // fingerprint -> id
    std::vector<Space> m_Spaces;
    std::vector<Run> m_Runs; // in creation order, which is the server's (created_at, run_id) order here
    int64_t m_NextResultId = 1;

    // As the server's EntityName: trimmed, at most 100 characters (not bytes), no U+0000. Trims ASCII whitespace only;
    // the server trims Unicode whitespace, which the tests do not send.
    static void CheckName(const std::string &name)
    {
        const size_t first = name.find_first_not_of(" \t\n\v\f\r");

        if (first == std::string::npos)
            return;

        const std::string trimmed = name.substr(first, name.find_last_not_of(" \t\n\v\f\r") - first + 1);

        if (trimmed.find('\0') != std::string::npos)
            throw std::invalid_argument("name contains U+0000");

        const auto characters = std::count_if(trimmed.begin(), trimmed.end(),
            [](const char byte) { return (static_cast<unsigned char>(byte) & 0xC0) != 0x80; });

        if (characters > 100)
            throw std::invalid_argument("name too long");
    }

    static bool HasHeader(const HttpHeaders &headers, const std::string &name)
    {
        return std::any_of(headers.begin(), headers.end(),
            [&name](const auto &header) { return header.first == name && !header.second.empty(); });
    }

    static HttpResponse Error(const long status, const std::string &message)
    {
        return HttpResponse{status, json{{"error", message}}.dump()};
    }

    static HttpResponse Ok(const json &body, const long status = 200)
    {
        return HttpResponse{status, body.dump()};
    }

    static std::vector<std::string> Split(const std::string &path)
    {
        std::vector<std::string> segments;
        size_t start = 1;

        while (start <= path.size())
        {
            const size_t end = path.find('/', start);
            segments.push_back(path.substr(start, end == std::string::npos ? std::string::npos : end - start));

            if (end == std::string::npos)
                break;

            start = end + 1;
        }

        return segments;
    }

    static std::map<std::string, std::string> ParseQuery(const std::string &query)
    {
        std::map<std::string, std::string> values;
        size_t start = 0;

        while (start < query.size())
        {
            const size_t end = std::min(query.find('&', start), query.size());
            const std::string item = query.substr(start, end - start);
            const size_t equals = item.find('=');
            values[item.substr(0, equals)] = equals == std::string::npos ? std::string() : item.substr(equals + 1);
            start = end + 1;
        }

        return values;
    }

    // Fingerprint: a decimal u64 as a string, without leading zeros.
    static const std::string &CheckFingerprint(const std::string &text)
    {
        if (text.empty() || text.size() > 20 || (text.size() > 1 && text[0] == '0')
            || text.find_first_not_of("0123456789") != std::string::npos)
            throw std::invalid_argument("invalid fingerprint");

        return text;
    }

    static void CheckLimit(const json &request, int64_t &limit)
    {
        if (request.contains("limit") && !request["limit"].is_null())
        {
            limit = request["limit"].get<int64_t>();

            if (limit < 1 || limit > 1000)
                throw std::invalid_argument("limit out of range");
        }
    }

    static void CheckDevice(const json &device)
    {
        static const std::set<std::string> Apis{"opencl", "cuda", "vulkan", "cpp"};

        for (const char *key : {"name", "vendor", "type"})
            device.at(key).get<std::string>();

        if (Apis.count(device.at("compute_api").get<std::string>()) == 0)
            throw std::invalid_argument("invalid compute_api");

        for (const char *key : {"device_identifier", "driver_version", "extensions"})
        {
            if (!device.at(key).is_null())
                device.at(key).get<std::string>();
        }

        for (const char *key : {"cuda_compute_capability_major", "cuda_compute_capability_minor"})
        {
            if (!device.at(key).is_null() && !device.at(key).is_number_integer())
                throw std::invalid_argument("invalid compute capability");
        }
    }

    // Checks a TuningInfo and returns its space info.
    static const json &CheckTuningInfo(const json &info)
    {
        CheckDevice(info.at("device"));

        if (info.at("tuner").at("name").get<std::string>().empty()
            || info.at("tuner").at("version").get<std::string>().empty())
            throw std::invalid_argument("empty tuner");

        if (!info.at("input_data").is_null())
            info.at("input_data").get<std::string>();

        const json &spaceInfo = info.at("space_info");

        for (const char *key : {"source_fingerprint", "parameter_fingerprint", "space_fingerprint"})
            CheckFingerprint(spaceInfo.at(key).get_ref<const std::string &>());

        for (const char *key : {"source_name", "space_name"})
        {
            if (!spaceInfo.at(key).is_null())
                CheckName(spaceInfo.at(key).get<std::string>());
        }

        return spaceInfo;
    }

    const Space *FindSpace(const std::string &source, const std::string &parameter, const std::string &space) const
    {
        for (const auto &candidate : m_Spaces)
        {
            if (candidate.sourceFingerprint == source && candidate.parameterFingerprint == parameter
                && candidate.spaceFingerprint == space)
                return &candidate;
        }

        return nullptr;
    }

    const Space *FindSpace(const std::string &source, const int64_t spaceId) const
    {
        for (const auto &candidate : m_Spaces)
        {
            if (candidate.id == spaceId && candidate.sourceFingerprint == source)
                return &candidate;
        }

        return nullptr;
    }

    static json ResultOut(const Run &run, const Result &result)
    {
        return json{{"duration", result.duration}, {"total_duration", result.totalDuration},
            {"output_format", run.outputFormat}, {"time_unit", run.timeUnit}, {"result", result.payload}};
    }

    // The results of the given runs, by duration and then result id, at most limit.
    static json ResultList(const std::vector<const Run *> &runs, const int64_t limit)
    {
        std::vector<std::pair<const Run *, const Result *>> results;

        for (const Run *run : runs)
        {
            for (const auto &result : run->results)
                results.emplace_back(run, &result);
        }

        std::sort(results.begin(), results.end(), [](const auto &left, const auto &right)
        {
            return std::tie(left.second->duration, left.second->id) < std::tie(right.second->duration, right.second->id);
        });

        json list = json::array();

        for (size_t i = 0; i < results.size() && static_cast<int64_t>(i) < limit; ++i)
            list.push_back(ResultOut(*results[i].first, *results[i].second));

        return json{{"results", std::move(list)}};
    }

    HttpResponse SaveRun(const json &request)
    {
        const std::string id = request.at("id").get<std::string>();

        if (UuidGenerator::GetVersion(UuidGenerator::Parse(id)) != 7 || id != UuidGenerator::ToString(UuidGenerator::Parse(id)))
            throw std::invalid_argument("id is not a lowercase UUIDv7");

        const json &info = request.at("tuning_info");
        const json &spaceInfo = CheckTuningInfo(info);
        const std::string format = request.at("output_format").get<std::string>();
        const std::string timeUnit = request.at("time_unit").get<std::string>();

        if (format != "json" && format != "json_t4")
            throw std::invalid_argument("invalid output_format");

        if (timeUnit != "nanoseconds" && timeUnit != "microseconds" && timeUnit != "milliseconds" && timeUnit != "seconds")
            throw std::invalid_argument("invalid time_unit");

        std::vector<Result> results;

        for (const auto &item : request.at("results"))
        {
            if (item.at("status") != "ok" || !item.at("configuration").is_object() || !item.at("result").is_object())
                throw std::invalid_argument("invalid result");

            results.push_back({0, item.at("duration").get<int64_t>(), item.at("total_duration").get<int64_t>(),
                item.at("result")});
        }

        const std::string sourceFingerprint = spaceInfo.at("source_fingerprint").get<std::string>();
        const auto source = m_Sources.emplace(sourceFingerprint, static_cast<int64_t>(m_Sources.size()) + 1).first;
        const Space *space = FindSpace(sourceFingerprint, spaceInfo.at("parameter_fingerprint").get<std::string>(),
            spaceInfo.at("space_fingerprint").get<std::string>());

        if (space == nullptr)
        {
            m_Spaces.push_back({static_cast<int64_t>(m_Spaces.size()) + 1, sourceFingerprint,
                spaceInfo.at("parameter_fingerprint").get<std::string>(),
                spaceInfo.at("space_fingerprint").get<std::string>()});
            space = &m_Spaces.back();
        }

        const auto existing = std::find_if(m_Runs.begin(), m_Runs.end(), [&id](const Run &run) { return run.id == id; });
        const bool created = existing == m_Runs.end();

        if (created)
        {
            for (auto &result : results)
                result.id = m_NextResultId++;

            m_Runs.push_back({id, space->id, info.at("device"), info.at("tuner"), info.at("input_data"), format,
                timeUnit, std::move(results)});
        }

        const Run &run = created ? m_Runs.back() : *existing;

        return Ok(json{{"run_id", id}, {"source_id", source->second}, {"space_id", run.spaceId},
            {"result_count", run.results.size()}, {"realm_ids", json::array({"00000000-0000-4000-8000-000000000001"})},
            {"source_has_code", false}}, created ? 201 : 200);
    }

    // The same device model, or for a CUDA query with both compute capabilities: a CUDA run with the same major and
    // a minor within +-1.
    static bool DeviceMatches(const json &query, const json &stored)
    {
        if (query.at("name") == stored.at("name") && query.at("vendor") == stored.at("vendor")
            && query.at("type") == stored.at("type"))
            return true;

        if (query.at("compute_api") != "cuda" || stored.at("compute_api") != "cuda"
            || query.at("cuda_compute_capability_major").is_null() || query.at("cuda_compute_capability_minor").is_null()
            || stored.at("cuda_compute_capability_major").is_null() || stored.at("cuda_compute_capability_minor").is_null())
            return false;

        const int64_t minor = query.at("cuda_compute_capability_minor").get<int64_t>();
        const int64_t storedMinor = stored.at("cuda_compute_capability_minor").get<int64_t>();

        return query.at("cuda_compute_capability_major") == stored.at("cuda_compute_capability_major")
            && storedMinor >= minor - 1 && storedMinor <= minor + 1;
    }

    HttpResponse BestResults(const json &request)
    {
        const json &info = request.at("tuning_info");
        const json &spaceInfo = CheckTuningInfo(info);
        int64_t limit = 50;
        CheckLimit(request, limit);

        const Space *space = FindSpace(spaceInfo.at("source_fingerprint").get<std::string>(),
            spaceInfo.at("parameter_fingerprint").get<std::string>(), spaceInfo.at("space_fingerprint").get<std::string>());
        std::vector<const Run *> runs;

        for (const auto &run : m_Runs)
        {
            if (space != nullptr && run.spaceId == space->id && DeviceMatches(info.at("device"), run.device))
                runs.push_back(&run);
        }

        return Ok(ResultList(runs, limit));
    }

    HttpResponse GetSpace(const std::string &fingerprint, const std::map<std::string, std::string> &query)
    {
        CheckFingerprint(fingerprint);
        const Space *space = FindSpace(fingerprint, CheckFingerprint(query.at("parameter_fingerprint")),
            CheckFingerprint(query.at("space_fingerprint")));

        if (space == nullptr)
            return Error(404, "space not found");

        return Ok(json{{"space_id", space->id}, {"source_id", m_Sources.at(fingerprint)}});
    }

    HttpResponse SpaceRuns(const std::string &fingerprint, const int64_t spaceId,
        const std::map<std::string, std::string> &query)
    {
        if (FindSpace(fingerprint, spaceId) == nullptr)
            return Error(404, "space not found");

        const size_t offset = query.count("offset") != 0 ? std::stoul(query.at("offset")) : 0;
        const size_t limit = query.count("limit") != 0 ? std::stoul(query.at("limit")) : 500;

        if (limit < 1 || limit > 500)
            return Error(400, "limit out of range");

        json runs = json::array();
        size_t index = 0;

        for (const auto &run : m_Runs)
        {
            if (run.spaceId != spaceId)
                continue;

            if (index++ < offset)
                continue;

            if (runs.size() == limit)
                break;

            runs.push_back(json{{"run_id", run.id}, {"input_data", run.inputData}, {"device", run.device},
                {"tuner", run.tuner}, {"output_format", run.outputFormat}, {"time_unit", run.timeUnit},
                {"created_at", "2026-11-06T10:00:00Z"}});
        }

        return Ok(json{{"runs", std::move(runs)}});
    }

    HttpResponse RunResults(const std::string &fingerprint, const int64_t spaceId, const json &request)
    {
        if (FindSpace(fingerprint, spaceId) == nullptr)
            return Error(404, "space not found");

        const json &ids = request.at("run_ids");

        if (!ids.is_array() || ids.empty() || ids.size() > 500)
            return Error(400, "run_ids must hold 1 to 500 ids");

        int64_t limit = 50;
        CheckLimit(request, limit);

        std::set<std::string> wanted;

        for (const auto &id : ids)
            wanted.insert(UuidGenerator::ToString(UuidGenerator::Parse(id.get<std::string>())));

        std::vector<const Run *> runs;

        for (const auto &run : m_Runs)
        {
            if (run.spaceId == spaceId && wanted.count(run.id) != 0)
                runs.push_back(&run);
        }

        return Ok(ResultList(runs, limit));
    }

    HttpResponse Stats(const std::string &fingerprint)
    {
        CheckFingerprint(fingerprint);
        std::set<int64_t> spaces;
        std::set<std::tuple<std::string, std::string, std::string>> devices;
        size_t runCount = 0;
        size_t resultCount = 0;

        for (const auto &run : m_Runs)
        {
            const auto space = std::find_if(m_Spaces.begin(), m_Spaces.end(),
                [&run](const Space &candidate) { return candidate.id == run.spaceId; });

            if (space->sourceFingerprint != fingerprint)
                continue;

            spaces.insert(run.spaceId);
            devices.emplace(run.device.at("name").get<std::string>(), run.device.at("vendor").get<std::string>(),
                run.device.at("type").get<std::string>());
            ++runCount;
            resultCount += run.results.size();
        }

        if (runCount == 0)
            return Error(404, "source not found");

        return Ok(json{{"space_count", spaces.size()}, {"device_count", devices.size()}, {"run_count", runCount},
            {"result_count", resultCount}});
    }
};

} // namespace ktt::db::test
