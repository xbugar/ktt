#pragma once

// Internal to the remote database client (not exported): value formats of the ktt-db application API.

#include <cstddef>
#include <string>

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

} // namespace ktt::db
