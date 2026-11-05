#include <catch.hpp>

#if defined(KTT_DATABASE)

#include <chrono>
#include <cstdint>
#include <set>

#include <Database/Utility/Uuid.h>

using ktt::db::uuid;
using ktt::db::UuidGenerator;

namespace
{

uint64_t TimestampMs(const uuid& id)
{
    uint64_t ms = 0;

    for (int i = 0; i < 6; ++i)
        ms = (ms << 8) | id[i];

    return ms;
}

uint64_t NowMs()
{
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
}

} // namespace

TEST_CASE("GenerateUuid produces a version 4 UUID with the RFC variant", "Uuid")
{
    const auto id = UuidGenerator::GenerateUuid();

    REQUIRE(UuidGenerator::GetVersion(id) == 4);
    REQUIRE((id[8] & 0xC0) == 0x80);
}

TEST_CASE("GenerateUuidV7 produces a version 7 UUID with the RFC variant", "Uuid")
{
    const auto id = UuidGenerator::GenerateUuidV7();

    REQUIRE(UuidGenerator::GetVersion(id) == 7);
    REQUIRE((id[8] & 0xC0) == 0x80);
}

TEST_CASE("GenerateUuidV7 embeds the current time in milliseconds", "Uuid")
{
    const auto before = NowMs();
    const auto id = UuidGenerator::GenerateUuidV7();
    const auto after = NowMs();

    REQUIRE(TimestampMs(id) >= before);
    REQUIRE(TimestampMs(id) <= after);
}

TEST_CASE("GenerateUuidV7 ids from different milliseconds sort in creation order", "Uuid")
{
    const auto first = UuidGenerator::GenerateUuidV7();
    const auto start = NowMs();

    while (NowMs() == start)
    {
    }

    const auto second = UuidGenerator::GenerateUuidV7();

    REQUIRE(first < second);
}

TEST_CASE("GenerateUuidV7 does not repeat", "Uuid")
{
    std::set<uuid> ids;

    for (int i = 0; i < 10000; ++i)
        ids.insert(UuidGenerator::GenerateUuidV7());

    REQUIRE(ids.size() == 10000);
}

#endif // KTT_DATABASE
