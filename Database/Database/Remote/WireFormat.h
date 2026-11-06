#pragma once

// Internal to the remote database client (not exported): value formats of the ktt-db application API.

#include <cstddef>
#include <string>

#include <Database/Utility/Uuid.h>

namespace ktt::db
{

/** @fn std::string FingerprintToWire(size_t fingerprint)
 * Fingerprints are sent as decimal strings, because a 64-bit value does not fit a JSON (JavaScript) number.
 * @return The fingerprint in decimal, e.g. "14695981039346656037".
 */
std::string FingerprintToWire(size_t fingerprint);

/** @fn size_t FingerprintFromWire(const std::string& text)
 * Parses a fingerprint sent by the server.
 * @param text Decimal digits only, without sign or whitespace.
 * @return The fingerprint.
 * @throw KttException If text is not a decimal number that fits size_t.
 */
size_t FingerprintFromWire(const std::string &text);

/** @fn std::string UuidToWire(const uuid& id)
 * @return The UUID in the lowercase 8-4-4-4-12 form, e.g. "0193a1f2-7c4e-7b3a-9d21-5e8f0c6a4b17".
 */
std::string UuidToWire(const uuid &id);

} // namespace ktt::db
