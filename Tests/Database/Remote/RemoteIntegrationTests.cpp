#include <catch.hpp>

#if defined(KTT_REMOTE_DATABASE)

// R8.4: against a running ktt-db server. Hidden (run it with: ./Tests "[integration]") and skipped unless both
// KTT_DB_URL and KTT_DB_API_KEY are set. The key needs the write permission. Every run uses new random fingerprints,
// so it can be repeated against the same server.

#include <cstdint>
#include <cstdlib>
#include <optional>
#include <random>
#include <string>

#include <Database/Remote/RemoteDatabase.h>
#include <Database/Utility/TimeUnitGuard.h>

#include "../DatabaseTestHelpers.h"

using ktt::db::test::MakeResult;
using ktt::db::test::MakeTuningInfo;
using ktt::db::test::Millisecond;

TEST_CASE("RemoteDatabase saves and reads results on a ktt-db server", "[.integration][Database][Remote]")
{
    const char *url = std::getenv("KTT_DB_URL");
    const char *apiKey = std::getenv("KTT_DB_API_KEY");

    if (url == nullptr || apiKey == nullptr || *url == '\0' || *apiKey == '\0')
    {
        WARN("KTT_DB_URL or KTT_DB_API_KEY is not set: skipped");
        return;
    }

    const ktt::db::RemoteDatabase db(url, apiKey);

    std::random_device random;
    std::uniform_int_distribution<size_t> fingerprints(1, SIZE_MAX);
    auto info = MakeTuningInfo();
    info.spaceInfo.sourceFingerprint = fingerprints(random);
    info.spaceInfo.parameterFingerprint = fingerprints(random);
    info.spaceInfo.spaceFingerprint = fingerprints(random);
    info.spaceInfo.sourceName = "ktt integration test";
    info.inputData = "integration";

    REQUIRE_FALSE(db.GetStatsForSource(info.spaceInfo.sourceFingerprint).has_value());
    REQUIRE(db.SimpleGetBestResults(info).empty());

    {
        const ktt::db::TimeUnitGuard unit(ktt::TimeUnit::Microseconds);
        db.SaveResults(info, {MakeResult("kernel", 64, 3 * Millisecond), MakeResult("kernel", 32, 2 * Millisecond)});
    }

    db.SaveResults(info, {MakeResult("kernel", 128, 1 * Millisecond)}, {ktt::OutputFormat::JSON_T4, 2});

    const auto best = db.SimpleGetBestResults(info, 2);
    REQUIRE(best.size() == 2);
    REQUIRE(best[0].GetKernelDuration() == 1 * Millisecond);
    REQUIRE(best[1].GetKernelDuration() == 2 * Millisecond);

    const auto filtered = db.GetBestResults({info.spaceInfo, std::nullopt,
        [](const std::string &input) { return input == "integration"; }, 10});
    REQUIRE(filtered.size() == 3);
    REQUIRE(filtered[2].GetKernelDuration() == 3 * Millisecond);

    const auto stats = db.GetStatsForSource(info.spaceInfo.sourceFingerprint);
    REQUIRE(stats.has_value());
    REQUIRE(stats->spaceCount == 1);
    REQUIRE(stats->deviceCount == 1);
    REQUIRE(stats->runCount == 2);
    REQUIRE(stats->resultCount == 3);
}

#endif // KTT_REMOTE_DATABASE
