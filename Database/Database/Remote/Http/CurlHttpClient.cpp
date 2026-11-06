#include <chrono>
#include <curl/curl.h>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

#include <Api/KttException.h>
#include <Database/Remote/Http/CurlHttpClient.h>
#include <KttPlatform.h>

namespace ktt::db
{

// curl_global_init is not thread-safe in older libcurl versions, so it runs once. curl_global_cleanup is never
// called: other code in the process may still use libcurl when KTT is unloaded.
static CURLcode InitializeCurlOnce()
{
    static std::once_flag flag;
    static CURLcode result = CURLE_OK;
    std::call_once(flag, [] { result = curl_global_init(CURL_GLOBAL_DEFAULT); });
    return result;
}

static size_t AppendToString(char *data, size_t size, size_t count, void *target)
{
    try
    {
        static_cast<std::string *>(target)->append(data, size * count);
        return size * count;
    }
    catch (...)
    {
        // Returning less than given makes libcurl abort the transfer with CURLE_WRITE_ERROR.
        return 0;
    }
}

// Connection errors and timeouts may go away on another attempt; a bad URL, a failed certificate check or a local
// error will not. A failed TLS handshake (CURLE_SSL_CONNECT_ERROR) is retried like a connection error, because a
// dropped connection during the handshake looks the same; a permanent handshake problem (e.g. https:// to a plain
// HTTP port) therefore only fails after the retries.
static bool IsRetryable(const CURLcode code)
{
    switch (code)
    {
    case CURLE_COULDNT_RESOLVE_PROXY:
    case CURLE_COULDNT_RESOLVE_HOST:
    case CURLE_COULDNT_CONNECT:
    case CURLE_OPERATION_TIMEDOUT:
    case CURLE_SSL_CONNECT_ERROR:
    case CURLE_SEND_ERROR:
    case CURLE_RECV_ERROR:
    case CURLE_GOT_NOTHING:
    case CURLE_PARTIAL_FILE:
    case CURLE_HTTP2:
    case CURLE_HTTP2_STREAM:
        return true;
    default:
        return false;
    }
}

template <typename Value>
static void SetOption(CURL *handle, const CURLoption option, Value value)
{
    if (const CURLcode code = curl_easy_setopt(handle, option, value); code != CURLE_OK)
        throw HttpTransportError(std::string("cannot configure the HTTP request: ") + curl_easy_strerror(code), false);
}

// 0 means no limit for libcurl. Longer values are clamped, because long has only 32 bits on Windows.
static long ToCurlMilliseconds(const std::chrono::milliseconds duration)
{
    if (duration.count() <= 0)
        return 0L;

    constexpr auto maxMilliseconds = static_cast<std::chrono::milliseconds::rep>(std::numeric_limits<long>::max());
    return duration.count() >= maxMilliseconds ? std::numeric_limits<long>::max() : static_cast<long>(duration.count());
}

CurlHttpClient::CurlHttpClient(const RemoteDatabaseOptions &options) :
    m_Options(options)
{
    if (const CURLcode code = InitializeCurlOnce(); code != CURLE_OK)
        throw KttException(std::string("Cannot initialize libcurl: ") + curl_easy_strerror(code));
}

HttpResponse CurlHttpClient::Send(const std::string_view method, const std::string &url, const HttpHeaders &headers,
    const std::string &body)
{
    const std::string methodName(method);

    // HEAD would need CURLOPT_NOBODY and other methods have not been checked, so they are refused rather than sent
    // in a way that may hang; a GET body would be dropped silently.
    if (methodName != "GET" && methodName != "POST" && methodName != "PUT" && methodName != "DELETE")
        throw HttpTransportError("unsupported HTTP method " + methodName, false);

    if (methodName == "GET" && !body.empty())
        throw HttpTransportError("a GET request cannot have a body", false);

    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> handle(curl_easy_init(), &curl_easy_cleanup);

    if (handle == nullptr)
        throw HttpTransportError("cannot create a libcurl handle", false);

    std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)> headerList(nullptr, &curl_slist_free_all);
    bool hasContentType = false;

    const auto appendHeader = [&headerList](const std::string &line) {
        curl_slist *extended = curl_slist_append(headerList.get(), line.c_str());

        if (extended == nullptr)
            throw HttpTransportError("cannot build the HTTP request headers", false);

        headerList.release();
        headerList.reset(extended);
    };

    for (const auto &[name, value] : headers)
    {
        appendHeader(name + ": " + value);
        hasContentType = hasContentType || curl_strequal(name.c_str(), "content-type");
    }

    // "Name:" with no value removes a header libcurl would add by itself: the form content type of a POST without
    // a body, and "Expect: 100-continue", which delays large bodies by a round trip.
    if (!hasContentType)
        appendHeader("Content-Type:");

    appendHeader("Expect:");

    CURL *curl = handle.get();
    char errorBuffer[CURL_ERROR_SIZE] = {};
    std::string responseBody;
    const std::string userAgent = "KTT/" + std::to_string(KTT_VERSION_MAJOR) + "." + std::to_string(KTT_VERSION_MINOR)
        + "." + std::to_string(KTT_VERSION_PATCH);

    SetOption(curl, CURLOPT_URL, url.c_str());
#if LIBCURL_VERSION_NUM >= 0x075500 // 7.85.0 deprecated CURLOPT_PROTOCOLS in favour of CURLOPT_PROTOCOLS_STR
    SetOption(curl, CURLOPT_PROTOCOLS_STR, "http,https");
#else
    SetOption(curl, CURLOPT_PROTOCOLS, static_cast<long>(CURLPROTO_HTTP | CURLPROTO_HTTPS));
#endif
    SetOption(curl, CURLOPT_FOLLOWLOCATION, 0L);
    SetOption(curl, CURLOPT_NOSIGNAL, 1L);
    SetOption(curl, CURLOPT_ERRORBUFFER, errorBuffer);
    SetOption(curl, CURLOPT_HTTPHEADER, headerList.get());
    SetOption(curl, CURLOPT_USERAGENT, userAgent.c_str());
    SetOption(curl, CURLOPT_ACCEPT_ENCODING, "");
    SetOption(curl, CURLOPT_CONNECTTIMEOUT_MS, ToCurlMilliseconds(m_Options.connectTimeout));
    SetOption(curl, CURLOPT_TIMEOUT_MS, ToCurlMilliseconds(m_Options.requestTimeout));
    SetOption(curl, CURLOPT_SSL_VERIFYPEER, m_Options.verifyTls ? 1L : 0L);
    SetOption(curl, CURLOPT_SSL_VERIFYHOST, m_Options.verifyTls ? 2L : 0L);

    const std::string caBundle = m_Options.caBundle ? m_Options.caBundle->string() : std::string();

    if (m_Options.caBundle)
        SetOption(curl, CURLOPT_CAINFO, caBundle.c_str());

    SetOption(curl, CURLOPT_WRITEFUNCTION, &AppendToString);
    SetOption(curl, CURLOPT_WRITEDATA, &responseBody);

    if (methodName == "GET")
    {
        SetOption(curl, CURLOPT_HTTPGET, 1L);
    }
    else
    {
        // POSTFIELDS sends the body (also an empty one, with Content-Length: 0); CUSTOMREQUEST replaces the method
        // name for PUT and DELETE.
        SetOption(curl, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(body.size()));
        SetOption(curl, CURLOPT_POSTFIELDS, body.c_str());

        if (methodName != "POST")
            SetOption(curl, CURLOPT_CUSTOMREQUEST, methodName.c_str());
    }

    if (const CURLcode code = curl_easy_perform(curl); code != CURLE_OK)
    {
        // The error buffer names the host or the TLS problem; it never has the request headers.
        std::string message = curl_easy_strerror(code);

        if (errorBuffer[0] != '\0')
            message += std::string(" (") + errorBuffer + ")";

        throw HttpTransportError(message, IsRetryable(code));
    }

    long status = 0;

    if (const CURLcode code = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status); code != CURLE_OK)
        throw HttpTransportError(std::string("cannot read the HTTP status: ") + curl_easy_strerror(code), false);

    return HttpResponse{status, std::move(responseBody)};
}

} // namespace ktt::db
