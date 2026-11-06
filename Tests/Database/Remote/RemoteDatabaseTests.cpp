#include <catch.hpp>

#if defined(KTT_REMOTE_DATABASE)

#include <memory>
#include <type_traits>

#include <Api/KttException.h>
#include <Database/Database.h>
#include <Database/Remote/RemoteDatabase.h>

#include "FakeHttpClient.h"

using ktt::db::RemoteDatabase;
using ktt::db::RemoteDatabaseOptions;

static_assert(std::is_base_of_v<ktt::db::IDatabase, RemoteDatabase>);
static_assert(!std::is_copy_constructible_v<RemoteDatabase>);
static_assert(!std::is_copy_assignable_v<RemoteDatabase>);

namespace
{

// A function that takes "a database" accepts both implementations (R2).
bool AcceptsDatabase(const ktt::db::IDatabase &)
{
    return true;
}

} // namespace

TEST_CASE("RemoteDatabaseOptions have the documented defaults", "[Database][Remote]")
{
    const RemoteDatabaseOptions options;

    REQUIRE(options.connectTimeout == std::chrono::milliseconds(5000));
    REQUIRE(options.requestTimeout == std::chrono::milliseconds(60000));
    REQUIRE(options.maxRetries == 3);
    REQUIRE(options.verifyTls);
    REQUIRE_FALSE(options.caBundle.has_value());
}

TEST_CASE("RemoteDatabase construction does not contact the server", "[Database][Remote]")
{
    const auto http = std::make_shared<ktt::db::test::FakeHttpClient>();
    const RemoteDatabase db("https://ktt.example.org", "kttak-test", {}, http);

    REQUIRE(http->Requests().empty());
    REQUIRE(AcceptsDatabase(db));
    REQUIRE(AcceptsDatabase(ktt::db::Database(std::filesystem::path(":memory:"))));
}

TEST_CASE("RemoteDatabase with libcurl is constructed without a request", "[Database][Remote]")
{
    // Nothing listens on this address; the constructor must still succeed because it sends nothing.
    REQUIRE_NOTHROW(RemoteDatabase("http://127.0.0.1:1", "kttak-test"));
}

TEST_CASE("RemoteDatabase rejects invalid arguments", "[Database][Remote]")
{
    const auto http = std::make_shared<ktt::db::test::FakeHttpClient>();

    REQUIRE_THROWS_AS(RemoteDatabase("ktt.example.org", "kttak-test", {}, http), ktt::KttException);
    REQUIRE_THROWS_AS(RemoteDatabase("https://ktt.example.org", "", {}, http), ktt::KttException);
    REQUIRE_THROWS_AS(RemoteDatabase("https://ktt.example.org", "kttak-test", {}, nullptr), ktt::KttException);
    REQUIRE_THROWS_AS(RemoteDatabase("ktt.example.org", "kttak-test"), ktt::KttException);
}

#endif // KTT_REMOTE_DATABASE
