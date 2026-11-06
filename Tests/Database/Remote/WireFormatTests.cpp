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

TEST_CASE("UUIDs are written lowercase 8-4-4-4-12", "[Database][Remote]")
{
    const ktt::db::uuid id{0x01, 0x93, 0xA1, 0xF2, 0x7C, 0x4E, 0x7B, 0x3A, 0x9D, 0x21, 0x5E, 0x8F, 0x0C, 0x6A, 0x4B, 0x17};
    REQUIRE(ktt::db::UuidToWire(id) == "0193a1f2-7c4e-7b3a-9d21-5e8f0c6a4b17");
}

#endif // KTT_REMOTE_DATABASE
