#ifndef UEARTHSCOPE_DATASELECT_AMBASSADOR_RATE_LIMITER_HPP
#define UEARTHSCOPE_DATASELECT_AMBASSADOR_RATE_LIMITER_HPP
#include <chrono>
#include <memory>
namespace UEarthscopeDataselectAmbassador
{

/// @class RateLimiter rateLimiter.hpp
/// @brief Utility to limit the number of requests per second to Earthscope. 
/// @copyright Ben Baker (University of Utah) distributed under the
///            MIT NO AI license.
class RateLimiter
{
public:
    /// @brief Constructs the rate limiter.
    /// @throws std::invalid_argument if requestsPerSecond is not postiive.
    explicit RateLimiter(const int requestsPerSecond);

    /// @result True indicates the request rate limit was exceeded.
    [[nodiscard]] bool rateExceeded(
        const std::chrono::steady_clock::time_point &now = std::chrono::steady_clock::now());
 
    /// @brief Destructor.
    ~RateLimiter();

    RateLimiter(const RateLimiter &) = delete;
    RateLimiter(RateLimiter &&) noexcept = delete;
    RateLimiter& operator=(const RateLimiter &) = delete;
    RateLimiter& operator=(RateLimiter &&) noexcept = delete;
private:
    class RateLimiterImpl;
    std::unique_ptr<RateLimiterImpl> pImpl;
};

}
#endif
 

