#include <chrono>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "noDataCache.hpp"

using namespace UEarthscopeDataselectAmbassador;

namespace
{
const NoDataCache::Clock::time_point T0{std::chrono::hours {1}};
constexpr std::chrono::seconds TTL{60};
const std::string KEY_A{
    "start=2026-11-18T21:16:17.134000&end=2026-11-18T21:21:17.134000"
    "&net=WY&sta=YGH&cha=HHZ&loc=01&format=miniseed&nodata=204"};
const std::string KEY_B{
    "start=2026-11-18T21:16:25.186000&end=2026-11-18T21:21:25.186000"
    "&net=RC&sta=CMI&cha=EHZ&loc=--&format=miniseed&nodata=204"};
}

TEST_CASE("UEarthscopeDataselectAmbassador::NoDataCache - construction",
          "[noDataCache]")
{
    SECTION("Time to live must be positive")
    {
        REQUIRE_THROWS_AS(NoDataCache(10, std::chrono::seconds {0}),
                          std::invalid_argument);
        REQUIRE_THROWS_AS(NoDataCache(10, std::chrono::seconds {-1}),
                          std::invalid_argument);
    }

    SECTION("Starts empty")
    {
        NoDataCache cache{10, TTL};
        REQUIRE(cache.size() == 0);
        REQUIRE_FALSE(cache.contains(KEY_A, T0));
    }
}

TEST_CASE("UEarthscopeDataselectAmbassador::NoDataCache - lookups",
          "[noDataCache]")
{
    NoDataCache cache{10, TTL};
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
        cache.insert(KEY_B, T0 + TTL + std::chrono::seconds {1});
        REQUIRE(cache.size() == 1);
        REQUIRE_FALSE(cache.contains(KEY_A, T0 + TTL + std::chrono::seconds {1}));
        REQUIRE(cache.contains(KEY_B, T0 + TTL + std::chrono::seconds {1}));
    }

    SECTION("Re-inserting refreshes the expiry without duplicating")
    {
        const auto later = T0 + std::chrono::seconds {30};
        cache.insert(KEY_A, later);
        REQUIRE(cache.size() == 1);
        REQUIRE(cache.contains(KEY_A, T0 + TTL));
        REQUIRE_FALSE(cache.contains(KEY_A, later + TTL));
    }
}

TEST_CASE("UEarthscopeDataselectAmbassador::NoDataCache - maximum entries",
          "[noDataCache]")
{
    SECTION("Full cache drops the oldest entry")
    {
        NoDataCache cache{3, TTL};
        for (int i = 0; i < 3; ++i)
        {
            cache.insert("key" + std::to_string(i),
                         T0 + std::chrono::seconds {i});
        }
        REQUIRE(cache.size() == 3);
        cache.insert("key3", T0 + std::chrono::seconds {3});
        REQUIRE(cache.size() == 3);
        const auto now = T0 + std::chrono::seconds {3};
        REQUIRE_FALSE(cache.contains("key0", now));
        REQUIRE(cache.contains("key1", now));
        REQUIRE(cache.contains("key2", now));
        REQUIRE(cache.contains("key3", now));
    }

    SECTION("Re-inserting moves an entry to the back of the line")
    {
        NoDataCache cache{2, TTL};
        cache.insert(KEY_A, T0);
        cache.insert(KEY_B, T0 + std::chrono::seconds {1});
        cache.insert(KEY_A, T0 + std::chrono::seconds {2});
        cache.insert("key", T0 + std::chrono::seconds {3});
        const auto now = T0 + std::chrono::seconds {3};
        REQUIRE(cache.size() == 2);
        REQUIRE(cache.contains(KEY_A, now));
        REQUIRE_FALSE(cache.contains(KEY_B, now));
        REQUIRE(cache.contains("key", now));
    }

    SECTION("Zero disables the cache")
    {
        NoDataCache cache{0, TTL};
        cache.insert(KEY_A, T0);
        REQUIRE(cache.size() == 0);
        REQUIRE_FALSE(cache.contains(KEY_A, T0));
    }

    SECTION("Never exceeds the maximum under concurrent use")
    {
        constexpr size_t maximum{50};
        NoDataCache cache{maximum, TTL};
        std::vector<std::thread> threads;
        for (int t = 0; t < 8; ++t)
        {
            threads.emplace_back([&cache, t]()
            {
                for (int i = 0; i < 1000; ++i)
                {
                    const auto key = std::to_string(t) + "-" + std::to_string(i % 100);
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
