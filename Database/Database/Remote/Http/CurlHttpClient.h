#pragma once

// Internal to the remote database client (not exported). Only CurlHttpClient.cpp includes curl.h.

#include <Database/Remote/Http/HttpClient.h>
#include <Database/Remote/RemoteDatabase.h>

namespace ktt::db
{

/** @class CurlHttpClient
 * HttpClient on top of libcurl. Every Send uses its own easy handle, so concurrent calls are safe.
 * TLS goes through the system's TLS stack, the certificate and host name are verified unless options.verifyTls is
 * false, and a proxy is taken from the environment (http_proxy in lower case only; https_proxy, all_proxy and
 * no_proxy in either case). Only http and https are allowed, and redirects are not followed, so the API key is
 * never sent to another host.
 */
class CurlHttpClient : public HttpClient
{
public:
    /** @fn explicit CurlHttpClient(const RemoteDatabaseOptions& options)
     * Initializes libcurl once per process.
     * @param options Timeouts and TLS settings; the retry settings are not used here.
     * @throw KttException If libcurl cannot be initialized.
     */
    explicit CurlHttpClient(const RemoteDatabaseOptions &options);

    HttpResponse Send(std::string_view method, const std::string &url, const HttpHeaders &headers,
        const std::string &body) override;

private:
    RemoteDatabaseOptions m_Options;
};

} // namespace ktt::db
