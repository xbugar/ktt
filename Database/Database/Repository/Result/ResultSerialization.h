#pragma once

#include <string>

#include <Api/Output/KernelResult.h>

namespace ktt::db
{

/** @fn std::string SerializeResultJson(const KernelResult &result, int indent)
 * Serializes a single kernel result into the regular JSON representation.
 * Defined in its own translation unit so the JsonConverters.h enum serializers do not collide with the T4 variant.
 * @param result Kernel result to serialize.
 * @param indent Indentation level passed to nlohmann::json::dump.
 * @return JSON string stored in the tuning_result.result column.
 */
std::string SerializeResultJson(const KernelResult &result, int indent);

/** @fn KernelResult DeserializeResultJson(const std::string &text)
 * Parses a stored regular JSON string back into a KernelResult.
 * @param text JSON string produced by SerializeResultJson.
 * @return Reconstructed kernel result.
 */
KernelResult DeserializeResultJson(const std::string &text);

/** @fn std::string SerializeResultJsonT4(const KernelResult &result, int indent)
 * Serializes a single kernel result into the JSON T4 representation.
 * Defined in its own translation unit so the JsonT4Converters.h enum serializers do not collide with the regular variant.
 * @param result Kernel result to serialize.
 * @param indent Indentation level passed to nlohmann::json::dump.
 * @return JSON T4 string stored in the tuning_result.result column.
 */
std::string SerializeResultJsonT4(const KernelResult &result, int indent);

/** @fn KernelResult DeserializeResultJsonT4(const std::string &text)
 * Parses a stored JSON T4 string back into a KernelResult.
 * @param text JSON T4 string produced by SerializeResultJsonT4.
 * @return Reconstructed kernel result.
 */
KernelResult DeserializeResultJsonT4(const std::string &text);

} // namespace ktt::db
