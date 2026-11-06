#pragma once

// Internal to the remote database client: not exported (no KTT_API) and not reachable from Database.h or
// RemoteDatabase.h. The HTTP library (libcurl) is hidden behind HttpClient, so that tests can replace it with a fake.

#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ktt::db
{

/** @typedef HttpHeaders
 * Request headers as name and value pairs, sent in this order.
 */
using HttpHeaders = std::vector<std::pair<std::string, std::string>>;

/** @struct HttpResponse
 * Status code and body of an HTTP response.
 */
struct HttpResponse
{
    long status{}; ///< HTTP status code, e.g. 200.
    std::string body; ///< Response body as received.
};

/** @class HttpTransportError
 * Thrown by HttpClient::Send when no HTTP response was received: a connection error, a timeout, a TLS failure or an
 * invalid URL. It never leaves the remote client: RemoteClient retries it or turns it into KttException.
 * The message must not contain request headers, because they carry the API key.
 */
class HttpTransportError : public std::runtime_error
{
public:
    /** @fn HttpTransportError(const std::string& message, bool retryable)
     * @param message What failed, without request headers.
     * @param retryable True for connection errors and timeouts, which are worth another attempt.
     */
    HttpTransportError(const std::string &message, bool retryable) :
        std::runtime_error(message),
        m_Retryable(retryable)
    {}

    /** @fn bool IsRetryable() const
     * @return True when the request may succeed if it is sent again (connection error or timeout).
     */
    bool IsRetryable() const
    {
        return m_Retryable;
    }

private:
    bool m_Retryable;
};

/** @class HttpClient
 * Sends one HTTP request and returns the response, whatever its status. Implementations must allow concurrent calls
 * of Send from several threads.
 */
class HttpClient
{
public:
    virtual ~HttpClient() = default;

    /** @fn virtual HttpResponse Send(std::string_view method, const std::string& url, const HttpHeaders& headers,
     * const std::string& body)
     * Sends one request. Does not retry and does not follow redirects.
     * @param method HTTP method: "GET", "POST", "PUT" or "DELETE". Others are refused with a non-retryable
     *        HttpTransportError.
     * @param url Absolute URL including the scheme.
     * @param headers Request headers.
     * @param body Request body; empty for none. A GET must not have one (non-retryable HttpTransportError).
     * @return The response, for every status code.
     * @throw HttpTransportError When no response was received.
     */
    virtual HttpResponse Send(std::string_view method, const std::string &url, const HttpHeaders &headers,
        const std::string &body) = 0;
};

} // namespace ktt::db
