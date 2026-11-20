#include <chrono>
#include <stdexcept>
#include <string>
#include <utility>
#include <catch2/catch_test_macros.hpp>
#include "uEarthscopeDataselectAmbassador/bulkRequest.hpp"
#include "uEarthscopeDataselectAmbassador/curlGlobal.hpp"
#include "uEarthscopeDataselectAmbassador/earthscopeClient.hpp"
#include "uEarthscopeDataselectAmbassador/earthscopeClientOptions.hpp"
#include "uEarthscopeDataselectAmbassador/request.hpp"
#include "fakeServer.hpp"

using namespace UEarthscopeDataselectAmbassador;

namespace
{
[[nodiscard]] EarthscopeClientOptions makeOptions()
{
    EarthscopeClientOptions options;
    // Port 1 on loopback: nothing listens, so connect is refused immediately
    options.setEndpoint("http://127.0.0.1:1/fdsnws/dataselect/1");
    options.setLegacyCredentials(std::pair {std::string {"fake.user"},
                                            std::string {"notARealPassword"}});
    options.setConnectTimeOut(std::chrono::seconds {2});
    options.setGetRequestTimeOut(std::chrono::seconds {2});
    options.setPostRequestTimeOut(std::chrono::seconds {2});
    return options;
}

[[nodiscard]] BulkRequest makeBulkRequest()
{
    return BulkRequest{"quality=B\n"
                       "IU ANMO 00 BHZ 2024-01-01T00:00:00 2024-01-01T00:10:00\n"
                       "UU CTU 01 HHZ 2024-01-01T00:00:00 2024-01-01T00:10:00\n"};
}

[[nodiscard]] Request makeRequest()
{
    return Request{"net=IU&sta=ANMO&loc=00&cha=BHZ"
                   "&start=2024-01-01T00:00:00&end=2024-01-01T00:10:00"};
}
}

TEST_CASE("UEarthscopeDataselectAmbassador::EarthscopeClient", "[client]")
{
    const CURLGlobal curlGlobal;

    SECTION("Requires credentials")
    {
        const EarthscopeClientOptions options;
        REQUIRE_THROWS_AS(EarthscopeClient(options, curlGlobal, nullptr),
                          std::invalid_argument);
    }

    SECTION("Initialized client attempts the transfer")
    {
        const EarthscopeClient client{makeOptions(), curlGlobal, nullptr};
        REQUIRE(client.isInitialized());
        const auto result = client.get(makeRequest());
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == EarthscopeClient::Error::ConnectionFailure);
    }

    SECTION("Token mode attempts the transfer too")
    {
        auto options = makeOptions();
        options.setAccessToken("fake.jwt.token");
        const EarthscopeClient client{options, curlGlobal, nullptr};
        const auto result = client.get(makeRequest());
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == EarthscopeClient::Error::ConnectionFailure);
    }
}

// EarthScope's origin sometimes sits ~21 s before the first byte.  The
// low-speed limit (no bytes for 5 s) should end each stalled attempt well
// before the request timeout, and the client should retry once.
TEST_CASE("UEarthscopeDataselectAmbassador::EarthscopeClient - stalls",
          "[client][stall]")
{
    using Testing::FakeServer;
    const CURLGlobal curlGlobal;
    auto options = makeOptions();
    options.setGetRequestTimeOut(std::chrono::seconds {30});
    options.setPostRequestTimeOut(std::chrono::seconds {30});

    SECTION("A stalled attempt is retried and the retry succeeds")
    {
        const FakeServer server{FakeServer::Mode::StallThenRespond};
        options.setEndpoint(server.endpoint());
        const EarthscopeClient client{options, curlGlobal, nullptr};
        const auto start = std::chrono::steady_clock::now();
        const auto result = client.get(makeRequest());
        const auto elapsed = std::chrono::steady_clock::now() - start;
        REQUIRE(result.has_value());
        REQUIRE(result->status == 200);
        REQUIRE(result->payload == FakeServer::PAYLOAD);
        REQUIRE(server.connections() == 2);
        REQUIRE(elapsed < std::chrono::seconds {10});
    }

    SECTION("A stalled POST is retried and the retry succeeds")
    {
        // Token auth is single pass; digest would add a probe connection
        const FakeServer server{FakeServer::Mode::StallThenRespond};
        options.setEndpoint(server.endpoint());
        options.setAccessToken("fake.jwt.token");
        const EarthscopeClient client{options, curlGlobal, nullptr};
        const auto result = client.post(makeBulkRequest());
        REQUIRE(result.has_value());
        REQUIRE(result->status == 200);
        REQUIRE(server.connections() == 2);
        REQUIRE(server.lastRequest().body == makeBulkRequest().toString());
    }

    SECTION("Two stalls give up as timed out, well before the request timeout")
    {
        const FakeServer server{FakeServer::Mode::Stall};
        options.setEndpoint(server.endpoint());
        const EarthscopeClient client{options, curlGlobal, nullptr};
        const auto start = std::chrono::steady_clock::now();
        const auto result = client.get(makeRequest());
        const auto elapsed = std::chrono::steady_clock::now() - start;
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == EarthscopeClient::Error::TimedOut);
        REQUIRE(server.connections() == 2);
        REQUIRE(elapsed < std::chrono::seconds {15});
    }
}

// What actually goes over the wire to EarthScope.
TEST_CASE("UEarthscopeDataselectAmbassador::EarthscopeClient - wire",
          "[client][wire]")
{
    using Testing::FakeServer;
    const CURLGlobal curlGlobal;
    auto options = makeOptions();
    const FakeServer server{FakeServer::Mode::Respond};
    options.setEndpoint(server.endpoint());
    const std::string path{"/fdsnws/dataselect/1/"};
    const std::string query{makeRequest().toString()};

    SECTION("Legacy credentials GET /queryauth with the canonical query")
    {
        const EarthscopeClient client{options, curlGlobal, nullptr};
        const auto result = client.get(makeRequest());
        REQUIRE(result.has_value());
        REQUIRE(result->status == 200);
        REQUIRE(result->payload == FakeServer::PAYLOAD);
        const auto request = server.lastRequest();
        REQUIRE(request.method == "GET");
        REQUIRE(request.target == path + "queryauth?" + query);
        REQUIRE(request.body.empty());
    }

    SECTION("A token GETs /query with the canonical query")
    {
        options.setAccessToken("fake.jwt.token");
        const EarthscopeClient client{options, curlGlobal, nullptr};
        const auto result = client.get(makeRequest());
        REQUIRE(result.has_value());
        const auto request = server.lastRequest();
        REQUIRE(request.method == "GET");
        REQUIRE(request.target == path + "query?" + query);
    }
}

TEST_CASE("UEarthscopeDataselectAmbassador::EarthscopeClient - POST wire",
          "[client][wire]")
{
    using Testing::FakeServer;
    const CURLGlobal curlGlobal;
    auto options = makeOptions();
    const FakeServer server{FakeServer::Mode::Respond};
    options.setEndpoint(server.endpoint());
    const std::string path{"/fdsnws/dataselect/1/"};
    const auto bulk = makeBulkRequest();

    SECTION("Legacy credentials POST the canonical body to /queryauth")
    {
        const EarthscopeClient client{options, curlGlobal, nullptr};
        const auto result = client.post(bulk);
        REQUIRE(result.has_value());
        REQUIRE(result->status == 200);
        REQUIRE(result->payload == FakeServer::PAYLOAD);
        const auto request = server.lastRequest();
        REQUIRE(request.method == "POST");
        REQUIRE(request.target == path + "queryauth");
        REQUIRE(request.body == bulk.toString());
        // Digest is multi-pass: curl probes with an empty POST before
        // sending the body, so the body isn't sent twice after a 401
        REQUIRE(server.connections() == 2);
        // Retired options never go upstream
        REQUIRE(request.body.find("quality") == std::string::npos);
    }

    SECTION("A token POSTs to /query")
    {
        options.setAccessToken("fake.jwt.token");
        const EarthscopeClient client{options, curlGlobal, nullptr};
        const auto result = client.post(bulk);
        REQUIRE(result.has_value());
        const auto request = server.lastRequest();
        REQUIRE(request.method == "POST");
        REQUIRE(request.target == path + "query");
        REQUIRE(request.body == bulk.toString());
        REQUIRE(server.connections() == 1);
    }

    SECTION("A GET after a POST on a new handle is still a GET")
    {
        const EarthscopeClient client{options, curlGlobal, nullptr};
        REQUIRE(client.post(bulk).has_value());
        REQUIRE(client.get(makeRequest()).has_value());
        const auto request = server.lastRequest();
        REQUIRE(request.method == "GET");
        REQUIRE(request.body.empty());
    }

    SECTION("An empty bulk request never reaches the server")
    {
        const EarthscopeClient client{options, curlGlobal, nullptr};
        const auto result = client.post(BulkRequest{});
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == EarthscopeClient::Error::InvalidRequest);
        REQUIRE(server.connections() == 0);
    }
}
