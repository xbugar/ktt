#pragma once

// Internal to the remote database client (not exported): the parts of RemoteDatabase.cpp that the other
// RemoteDatabase*.cpp files share. They are defined in RemoteDatabase.cpp.

#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include <json.hpp>

#include <Api/Info/DatabaseTuningInfo.h>
#include <Output/OutputFormat.h>

namespace ktt::db
{

class RemoteClient;

namespace remote
{

/** @fn std::optional<nlohmann::json> CallServer(const RemoteClient& client, const std::string& operation,
 * std::string_view method, const std::string& path, const nlohmann::json* body, bool notFoundIsEmpty)
 * Sends one request (retried as RetryPolicy::Idempotent) and returns its JSON body.
 * @return The body of a 200 or 201 response; std::nullopt for a ktt-db 404 when notFoundIsEmpty.
 * @throw KttException For every other status, a network error, or a success body that is not a JSON object. The
 *        message has the operation, the request, the status and the server's error text, never the API key.
 */
std::optional<nlohmann::json> CallServer(const RemoteClient &client, const std::string &operation,
    std::string_view method, const std::string &path, const nlohmann::json *body, bool notFoundIsEmpty);

/** @fn void RunTranslated(const std::string& operation, const RemoteClient& client, const std::function<void()>& function)
 * Runs the body of a public method so that a JSON error or a KttException from the conversions becomes a
 * KttException that starts with the operation and has no API key in it.
 */
void RunTranslated(const std::string &operation, const RemoteClient &client, const std::function<void()> &function);

/** @fn DeviceInfo ReadDevice(const nlohmann::json& device)
 * Reads a DeviceInfo sent by the server, the way the local RunRepository reads a stored device.
 */
DeviceInfo ReadDevice(const nlohmann::json &device);

/** @fn OutputFormat ReadOutputFormat(const std::string& name)
 * Reads an output_format value ("json" or "json_t4").
 * @throw KttException For any other value.
 */
OutputFormat ReadOutputFormat(const std::string &name);

} // namespace remote

} // namespace ktt::db
