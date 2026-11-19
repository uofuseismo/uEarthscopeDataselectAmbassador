#include <atomic>
#include <chrono>
#include <stdexcept>
#include <thread>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "uEarthscopeDataselectAmbassador/rateLimiter.hpp"

using namespace UEarthscopeDataselectAmbassador;

namespace
{
const std::chrono::steady_clock::time_point T0{std::chrono::hours {1}};
constexpr int RATE{20};

[[nodiscard]] int acquireMany(RateLimiter &limiter, const int attempts,
                              const std::chrono::steady_clock::time_point now)
{
    int granted{0};
    for (int i = 0; i < attempts; ++i)
    {
        if (!limiter.rateExceeded(now)){++granted;}
    }
    return granted;
}
}

TEST_CASE("UEarthscopeDataselectAmbassador::RateLimiter - construction",
          "[rateLimiter]")
{
    REQUIRE_THROWS_AS(RateLimiter(0), std::invalid_argument);
    REQUIRE_THROWS_AS(RateLimiter(-1), std::invalid_argument);
    REQUIRE_NOTHROW(RateLimiter(1));
}

TEST_CASE("UEarthscopeDataselectAmbassador::RateLimiter - window",
          "[rateLimiter]")
{
    RateLimiter limiter{RATE};

    SECTION("A burst up to the rate is allowed, then denied")
    {
        REQUIRE(acquireMany(limiter, RATE, T0) == RATE);
        REQUIRE(limiter.rateExceeded(T0));
        REQUIRE(limiter.rateExceeded(T0 + std::chrono::milliseconds {999}));
    }

    SECTION("The window reopens one second later")
    {
        REQUIRE(acquireMany(limiter, RATE, T0) == RATE);
        REQUIRE(acquireMany(limiter, RATE + 5, T0 + std::chrono::seconds {1})
             == RATE);
    }

    SECTION("The window slides rather than resetting")
    {
        const auto half = T0 + std::chrono::milliseconds {500};
        REQUIRE(acquireMany(limiter, RATE/2, T0) == RATE/2);
        REQUIRE(acquireMany(limiter, RATE/2, half) == RATE/2);
        // Only the first half has aged out
        REQUIRE(acquireMany(limiter, RATE, T0 + std::chrono::seconds {1})
             == RATE/2);
        REQUIRE(acquireMany(limiter, RATE, half + std::chrono::seconds {1})
             == RATE/2);
    }

    SECTION("Denied attempts don't count against later requests")
    {
        REQUIRE(acquireMany(limiter, RATE, T0) == RATE);
        for (int i = 1; i < 1000; ++i)
        {
            static_cast<void> (
                limiter.rateExceeded(T0 + std::chrono::milliseconds {i}));
        }
        REQUIRE(acquireMany(limiter, RATE, T0 + std::chrono::seconds {1})
             == RATE);
    }

    SECTION("Sustained overload still lets the rate through")
    {
        // An attempt every millisecond for 10 s is 50x the rate
        int granted{0};
        for (int i = 0; i < 10000; ++i)
        {
            if (!limiter.rateExceeded(T0 + std::chrono::milliseconds {i}))
            {
                ++granted;
            }
        }
        REQUIRE(granted == 10*RATE);
    }

    SECTION("Requests spread under the rate are never denied")
    {
        // 10 per second for 10 s
        for (int i = 0; i < 100; ++i)
        {
            REQUIRE_FALSE(limiter.rateExceeded(T0 + std::chrono::milliseconds {100*i}));
        }
    }

    SECTION("Default clock")
    {
        RateLimiter live{2};
        REQUIRE_FALSE(live.rateExceeded());
        REQUIRE_FALSE(live.rateExceeded());
        REQUIRE(live.rateExceeded());
    }

}

TEST_CASE("UEarthscopeDataselectAmbassador::RateLimiter - concurrency",
          "[rateLimiter]")
{
    RateLimiter limiter{RATE};
    std::atomic<int> granted{0};
    std::vector<std::thread> threads;
    threads.reserve(8);
    for (int t = 0; t < 8; ++t)
    {
        threads.emplace_back([&limiter, &granted]()
        {
            for (int i = 0; i < 1000; ++i)
            {
                if (!limiter.rateExceeded(T0)){++granted;}
            }
        });
    }
    for (auto &thread : threads){thread.join();}
    REQUIRE(granted.load() == RATE);
}
