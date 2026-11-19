#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace ktt::db
{
using uuid = std::array<uint8_t, 16>;

class UuidGenerator
{
public:
    /** @fn static uuid GenerateUuid()
     * Generates a random (version 4) UUID. Used for runs created in a local SQLite database.
     */
    static uuid GenerateUuid();

    /** @fn static uuid GenerateUuidV7()
     * Generates a time-ordered (version 7, RFC 9562) UUID: a 48-bit big-endian Unix timestamp in milliseconds,
     * followed by the version, 12 random bits, the variant and 62 random bits. Used for runs created through the
     * remote (server) database, so that the version tells a server run (7) from a local one (4).
     */
    static uuid GenerateUuidV7();

    /** @fn static uint8_t GetVersion(const uuid& id)
     * Returns the version of the UUID (the high nibble of byte 6), e.g. 4 or 7.
     */
    static uint8_t GetVersion(const uuid& id);

    /** @fn static std::string ToString(const uuid& id)
     * Formats the UUID in the lowercase 8-4-4-4-12 form used on the wire, e.g. "0193a1f2-7c4e-7b3a-9d21-5e8f0c6a4b17".
     */
    static std::string ToString(const uuid& id);

    /** @fn static uuid Parse(const std::string& text)
     * Parses a UUID in the 8-4-4-4-12 form. Hex digits may be lowercase or uppercase. Inverse of ToString().
     * @throw KttException If text is not a UUID in that form.
     */
    static uuid Parse(const std::string& text);
};

} // namespace ktt::db
