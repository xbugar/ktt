#pragma once

// Fake HttpClient for the remote database tests (no network). Included only inside the KTT_REMOTE_DATABASE guard.
// It records every request and answers from a queue of scripted outcomes; when the queue is empty it answers with the
// fallback response. Thread-safe, like the real client.

#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <Database/Remote/Http/HttpClient.h>

namespace ktt::db::test
{

struct RecordedRequest
{
    std::string method;
    std::string url;
    HttpHeaders headers;
    std::string body;

    // Value of the first header with this (lower-case) name.
    std::optional<std::string> Header(const std::string &name) const
    {
        for (const auto &[key, value] : headers)
        {
            if (key == name)
                return value;
        }

        return std::nullopt;
    }
};

class FakeHttpClient : public HttpClient
{
public:
    using Outcome = std::variant<HttpResponse, HttpTransportError>;

    void Enqueue(Outcome outcome)
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_Outcomes.push_back(std::move(outcome));
    }

    void EnqueueResponse(const long status, std::string body = {})
    {
        Enqueue(HttpResponse{status, std::move(body)});
    }

    void EnqueueTransportError(const std::string &message, const bool retryable)
    {
        Enqueue(HttpTransportError(message, retryable));
    }

    HttpResponse Send(std::string_view method, const std::string &url, const HttpHeaders &headers,
        const std::string &body) override
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_Requests.push_back(RecordedRequest{std::string(method), url, headers, body});

        if (m_Outcomes.empty())
            return m_Fallback;

        Outcome outcome = std::move(m_Outcomes.front());
        m_Outcomes.pop_front();

        if (auto *error = std::get_if<HttpTransportError>(&outcome))
            throw *error;

        return std::get<HttpResponse>(outcome);
    }

    std::vector<RecordedRequest> Requests() const
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_Requests;
    }

private:
    mutable std::mutex m_Mutex;
    std::deque<Outcome> m_Outcomes;
    std::vector<RecordedRequest> m_Requests;
    HttpResponse m_Fallback{200, "{}"};
};

} // namespace ktt::db::test
