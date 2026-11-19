#include <chrono>
#include <stdexcept>
#include <string>
#include <utility>
#include <catch2/catch_test_macros.hpp>
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
    options.setRequestTimeOut(std::chrono::seconds {2});
    return options;
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
    options.setRequestTimeOut(std::chrono::seconds {30});

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
