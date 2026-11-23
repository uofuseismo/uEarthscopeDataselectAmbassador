#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <deque>
#include "uEarthscopeDataselectAmbassador/rateLimiter.hpp"

using namespace UEarthscopeDataselectAmbassador;

class RateLimiter::RateLimiterImpl
{
public:
    RateLimiterImpl(const int requestsPerSecond)
    {
        if (requestsPerSecond < 1)
        {
            throw std::invalid_argument("Requests per second must be positive");
        }
        mMaximumRequestsPerSecond = static_cast<size_t> (requestsPerSecond);
    }

    [[nodiscard]] bool rateExceeded(
        const std::chrono::steady_clock::time_point &now)
    {
        constexpr std::chrono::seconds window{1};
        const std::scoped_lock lock{mMutex};
        while (!mRequests.empty() && now - mRequests.front() >= window)
        {
            mRequests.pop_front();
        }
        if (mRequests.size() >= mMaximumRequestsPerSecond){return true;}
        mRequests.push_back(now);
        return false;
    }

    mutable std::mutex mMutex;
    mutable std::deque<std::chrono::steady_clock::time_point> mRequests; 
    size_t mMaximumRequestsPerSecond{0};
};

/// Constructor
RateLimiter::RateLimiter(const int requestsPerSecond) :
    pImpl(std::make_unique<RateLimiterImpl> (requestsPerSecond))
{
}

bool RateLimiter::rateExceeded(
    const std::chrono::steady_clock::time_point &now)
{
    return pImpl->rateExceeded(now);
}

/// Destructor
RateLimiter::~RateLimiter() = default;
