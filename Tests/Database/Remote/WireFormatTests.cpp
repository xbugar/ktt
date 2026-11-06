#include <catch.hpp>

#if defined(KTT_REMOTE_DATABASE)

#include <cstddef>
#include <limits>
#include <string>

#include <Api/KttException.h>
#include <Database/Remote/WireFormat.h>

using ktt::db::FingerprintFromWire;
using ktt::db::FingerprintToWire;

TEST_CASE("Fingerprints are decimal strings on the wire", "[Database][Remote]")
{
    REQUIRE(FingerprintToWire(0) == "0");
    REQUIRE(FingerprintToWire(14695981039346656037ULL) == "14695981039346656037");

    for (const size_t value : {size_t{0}, size_t{1}, size_t{1099511628211ULL}, std::numeric_limits<size_t>::max()})
        REQUIRE(FingerprintFromWire(FingerprintToWire(value)) == value);
}

TEST_CASE("Invalid fingerprint strings are rejected", "[Database][Remote]")
{
    for (const std::string text : {"", "-1", "+1", " 1", "1 ", "12a", "0x10", "1.5", "18446744073709551616",
             "99999999999999999999999"})
    {
        CAPTURE(text);
        REQUIRE_THROWS_AS(FingerprintFromWire(text), ktt::KttException);
    }
}

#endif // KTT_REMOTE_DATABASE
