#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include "uEarthscopeDataselectAmbassador/noDataCache.hpp"
#include "uEarthscopeDataselectAmbassador/noDataCacheOptions.hpp"
#include "temporaryDirectory.hpp"

using namespace UEarthscopeDataselectAmbassador;
using Testing::TemporaryDirectory;

namespace
{
const std::chrono::steady_clock::time_point T0{std::chrono::hours {1}};
constexpr std::chrono::seconds TTL{60};
const std::string KEY_A{
    "start=2026-11-18T21:16:17.134000&end=2026-11-18T21:21:17.134000"
    "&net=WY&sta=YGH&cha=HHZ&loc=01&format=miniseed&nodata=204"};
const std::string KEY_B{
    "start=2026-11-18T21:16:25.186000&end=2026-11-18T21:21:25.186000"
    "&net=RC&sta=CMI&cha=EHZ&loc=--&format=miniseed&nodata=204"};

[[nodiscard]] NoDataCacheOptions makeOptions(const uint16_t maximumEntries)
{
    NoDataCacheOptions options;
    options.setTimeToLive(TTL);
    options.setMaximumNumberOfEntries(maximumEntries);
    return options;
}
}

TEST_CASE("UEarthscopeDataselectAmbassador::NoDataCacheOptions",
          "[noDataCache]")
{
    SECTION("Defaults")
    {
        const NoDataCacheOptions options;
        REQUIRE(options.getTimeToLive() == std::chrono::seconds {150});
        REQUIRE(options.getMaximumNumberOfEntries() == 4096);
    }

    SECTION("Setters")
    {
        const auto options = makeOptions(10);
        REQUIRE(options.getTimeToLive() == TTL);
        REQUIRE(options.getMaximumNumberOfEntries() == 10);
    }

    SECTION("Time to live must be positive")
    {
        NoDataCacheOptions options;
        REQUIRE_THROWS_AS(options.setTimeToLive(std::chrono::seconds {0}),
                          std::invalid_argument);
        REQUIRE_THROWS_AS(options.setTimeToLive(std::chrono::seconds {-1}),
                          std::invalid_argument);
        REQUIRE(options.getTimeToLive() == std::chrono::seconds {150});
    }

    SECTION("Maximum entries range")
    {
        NoDataCacheOptions options;
        options.setMaximumNumberOfEntries(0);
        REQUIRE(options.getMaximumNumberOfEntries() == 0);
        options.setMaximumNumberOfEntries(65535);
        REQUIRE(options.getMaximumNumberOfEntries() == 65535);
        REQUIRE_THROWS_AS(options.setMaximumNumberOfEntries(-1),
                          std::invalid_argument);
        REQUIRE_THROWS_AS(options.setMaximumNumberOfEntries(65536),
                          std::invalid_argument);
        REQUIRE(options.getMaximumNumberOfEntries() == 65535);
    }

    SECTION("Copy and move")
    {
        const auto options = makeOptions(10);
        const NoDataCacheOptions copy{options}; // NOLINT(performance-unnecessary-copy-initialization)
        REQUIRE(copy.getTimeToLive() == TTL);
        REQUIRE(copy.getMaximumNumberOfEntries() == 10);

        NoDataCacheOptions assigned;
        assigned = options;
        REQUIRE(assigned.getMaximumNumberOfEntries() == 10);

        auto source = makeOptions(11);
        const NoDataCacheOptions moved{std::move(source)};
        REQUIRE(moved.getTimeToLive() == TTL);
        REQUIRE(moved.getMaximumNumberOfEntries() == 11);
    }
}

TEST_CASE("UEarthscopeDataselectAmbassador::NoDataCache - lookups",
          "[noDataCache]")
{
    NoDataCache cache{makeOptions(10)};
    REQUIRE(cache.size() == 0);
    REQUIRE_FALSE(cache.contains(KEY_A, T0));
    cache.insert(KEY_A, T0);

    SECTION("Hit on the exact key only")
    {
        REQUIRE(cache.contains(KEY_A, T0));
        REQUIRE_FALSE(cache.contains(KEY_B, T0));
        REQUIRE_FALSE(cache.contains(KEY_A + "x", T0));
        REQUIRE(cache.size() == 1);
    }

    SECTION("Still a hit just before the time to live")
    {
        REQUIRE(cache.contains(KEY_A, T0 + TTL - std::chrono::milliseconds {1}));
    }

    SECTION("Expires at the time to live")
    {
        REQUIRE_FALSE(cache.contains(KEY_A, T0 + TTL));
        REQUIRE(cache.size() == 0);
    }

    SECTION("Expired entries are dropped on insert")
    {
        const auto later = T0 + TTL + std::chrono::seconds {1};
        cache.insert(KEY_B, later);
        REQUIRE(cache.size() == 1);
        REQUIRE_FALSE(cache.contains(KEY_A, later));
        REQUIRE(cache.contains(KEY_B, later));
    }

    SECTION("Re-inserting refreshes the expiry without duplicating")
    {
        const auto later = T0 + std::chrono::seconds {30};
        cache.insert(KEY_A, later);
        REQUIRE(cache.size() == 1);
        REQUIRE(cache.contains(KEY_A, T0 + TTL));
        REQUIRE_FALSE(cache.contains(KEY_A, later + TTL));
    }

    SECTION("Default clock")
    {
        NoDataCache liveCache{makeOptions(10)};
        liveCache.insert(KEY_A);
        REQUIRE(liveCache.contains(KEY_A));
        REQUIRE_FALSE(liveCache.contains(KEY_B));
    }
}

TEST_CASE("UEarthscopeDataselectAmbassador::NoDataCache - maximum entries",
          "[noDataCache]")
{
    SECTION("Full cache drops the oldest entry")
    {
        NoDataCache cache{makeOptions(3)};
        for (int i = 0; i < 3; ++i)
        {
            cache.insert("key" + std::to_string(i),
                         T0 + std::chrono::seconds {i});
        }
        REQUIRE(cache.size() == 3);
        const auto now = T0 + std::chrono::seconds {3};
        cache.insert("key3", now);
        REQUIRE(cache.size() == 3);
        REQUIRE_FALSE(cache.contains("key0", now));
        REQUIRE(cache.contains("key1", now));
        REQUIRE(cache.contains("key2", now));
        REQUIRE(cache.contains("key3", now));
    }

    SECTION("Re-inserting moves an entry to the back of the line")
    {
        NoDataCache cache{makeOptions(2)};
        cache.insert(KEY_A, T0);
        cache.insert(KEY_B, T0 + std::chrono::seconds {1});
        cache.insert(KEY_A, T0 + std::chrono::seconds {2});
        const auto now = T0 + std::chrono::seconds {3};
        cache.insert("key", now);
        REQUIRE(cache.size() == 2);
        REQUIRE(cache.contains(KEY_A, now));
        REQUIRE_FALSE(cache.contains(KEY_B, now));
        REQUIRE(cache.contains("key", now));
    }

    SECTION("Zero disables the cache")
    {
        NoDataCache cache{makeOptions(0)};
        cache.insert(KEY_A, T0);
        REQUIRE(cache.size() == 0);
        REQUIRE_FALSE(cache.contains(KEY_A, T0));
    }

    SECTION("Never exceeds the maximum under concurrent use")
    {
        constexpr uint16_t maximum{50};
        NoDataCache cache{makeOptions(maximum)};
        std::vector<std::thread> threads;
        threads.reserve(8);
        for (int t = 0; t < 8; ++t)
        {
            threads.emplace_back([&cache, t]()
            {
                for (int i = 0; i < 1000; ++i)
                {
                    const auto key = std::to_string(t) + "-"
                                   + std::to_string(i % 100);
                    cache.insert(key, T0);
                    static_cast<void> (cache.contains(key, T0));
                }
            });
        }
        for (auto &thread : threads){thread.join();}
        REQUIRE(cache.size() <= maximum);
        REQUIRE(cache.size() > 0);
    }
}

TEST_CASE("UEarthscopeDataselectAmbassador::NoDataCacheOptions - "
          "initialization file", "[noDataCache][ini]")
{
    const TemporaryDirectory directory;

    SECTION("Missing file")
    {
        REQUIRE_THROWS_AS(
            NoDataCacheOptions::fromInitializationFile(
                directory.path() / "doesNotExist.ini"),
            std::invalid_argument);
    }

    SECTION("Missing section gives the defaults")
    {
        const auto ini = directory.write("test.ini",
            "[Earthscope]\n"
            "endpoint = http://localhost:8080/fdsnws/dataselect/1\n");
        const auto options = NoDataCacheOptions::fromInitializationFile(ini);
        REQUIRE(options.getTimeToLive() == std::chrono::seconds {150});
        REQUIRE(options.getMaximumNumberOfEntries() == 4096);
    }

    SECTION("All settings")
    {
        const auto ini = directory.write("test.ini",
            "[NoDataCache]\n"
            "maximumEntries = 10000\n"
            "timeToLiveInSeconds = 300\n");
        const auto options = NoDataCacheOptions::fromInitializationFile(ini);
        REQUIRE(options.getTimeToLive() == std::chrono::seconds {300});
        REQUIRE(options.getMaximumNumberOfEntries() == 10000);
    }

    SECTION("Zero entries disables the cache")
    {
        const auto ini = directory.write("test.ini",
            "[NoDataCache]\n"
            "maximumEntries = 0\n");
        const auto options = NoDataCacheOptions::fromInitializationFile(ini);
        REQUIRE(options.getMaximumNumberOfEntries() == 0);
    }

    SECTION("Custom section")
    {
        const auto ini = directory.write("test.ini",
            "[NoDataCache]\n"
            "timeToLiveInSeconds = 300\n"
            "[NoDataCacheStaging]\n"
            "timeToLiveInSeconds = 30\n");
        const auto options = NoDataCacheOptions::fromInitializationFile(
            ini, "NoDataCacheStaging");
        REQUIRE(options.getTimeToLive() == std::chrono::seconds {30});
    }

    // Values outside 0-65535 must not wrap through the uint16_t setter
    SECTION("Bad settings are rejected")
    {
        auto setting = GENERATE(as<std::string> {},
                                "timeToLiveInSeconds = 0",
                                "timeToLiveInSeconds = -1",
                                "maximumEntries = -1",
                                "maximumEntries = 70000");
        const auto ini = directory.write("test.ini",
            "[NoDataCache]\n" + setting + "\n");
        REQUIRE_THROWS_AS(NoDataCacheOptions::fromInitializationFile(ini),
                          std::invalid_argument);
    }
}
