#include <cerrno>
#include <cstdlib>
#include <limits>

#include <Api/KttException.h>
#include <Database/Remote/WireFormat.h>

namespace ktt::db
{

std::string FingerprintToWire(const size_t fingerprint)
{
    return std::to_string(fingerprint);
}

size_t FingerprintFromWire(const std::string &text)
{
    // strtoull alone would accept leading whitespace, a sign ("-1" wraps around) and trailing characters.
    bool digitsOnly = !text.empty();

    for (const char c : text)
        digitsOnly = digitsOnly && c >= '0' && c <= '9';

    if (!digitsOnly)
        throw KttException("Invalid fingerprint received from the server: \"" + text + "\"");

    errno = 0;
    const unsigned long long value = std::strtoull(text.c_str(), nullptr, 10);

    if (errno == ERANGE || value > std::numeric_limits<size_t>::max())
        throw KttException("Fingerprint received from the server is out of range: " + text);

    return static_cast<size_t>(value);
}

std::string UuidToWire(const uuid &id)
{
    static constexpr char HexDigits[] = "0123456789abcdef";
    std::string text;
    text.reserve(36);

    for (size_t i = 0; i < id.size(); ++i)
    {
        if (i == 4 || i == 6 || i == 8 || i == 10)
            text += '-';

        text += HexDigits[id[i] >> 4];
        text += HexDigits[id[i] & 0x0F];
    }

    return text;
}

} // namespace ktt::db
