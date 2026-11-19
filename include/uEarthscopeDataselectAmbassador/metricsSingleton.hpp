#ifndef UEARTHSCOPE_DATASELECT_AMBASSADOR_METRICS_SINGLETON_HPP
#define UEARTHSCOPE_DATASELECT_AMBASSADOR_METRICS_SINGLETON_HPP
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <map>
namespace UEarthscopeDataselectAmbassador::Metrics
{
/// @class MetricsSingleton metricsSingleton.hpp
/// @brief A globally accessible point from which to read and write application
///        metrics.
/// @note This should be instantiated at application startup.
///       @sa \initializeMetricsSingleton().
/// @copyright Ben Baker (University of Utah) distributed under the
///            MIT NO AI license.
class MetricsSingleton
{
public:
    /// @result An instance of the singleton.
    [[maybe_unused]] static MetricsSingleton &getInstance();

    /// @brief Increments the 500 error for this route.
    void incrementServerErrorCounter(const std::string &route);
    /// @result The error counts for the utilized routes.
    [[nodiscard]] std::map<std::string, int64_t> getServerErrorCounters() const; 

    /// @brief Increments the 400 error for this route.
    void incrementClientErrorCounter(const std::string &route);
    /// @result The client error counts for the utilized routes.
    [[nodiscard]] std::map<std::string, int64_t> getClientErrorCounters() const;

    /// @brief Counts our unauthorized requests to Earthscope.
    void incrementClientUnauthorizedCounter() noexcept;
    /// @result The number of unauthorized request to Earthscope.
    [[nodiscard]] int64_t getClientUnauthorizedCounts() const noexcept;

    /// @brief Counts the number of unauthenticated requests to Earthscope.
    void incrementClientUnauthenticatedCounter() noexcept;
    /// @result The number of unauthenticated attempts to EArthscope.
    [[nodiscard]] int64_t getClientUnauthenticatedCounts() const noexcept;

    /// @brief Updates the number of bytes downloaded from Earthscope.
    void incrementBytesDownloadedCounter(const uint32_t nBytes);
    /// @result The number of bytes received.
    [[nodiscard]] int64_t getBytesDownloadedCount() const noexcept;
  
    /// @brief Increments the number of Earthscope requests.
    void incrementEarthscopeRequestsCounter() noexcept;
    /// @result The number of Earthscope requests. 
    [[nodiscard]] int64_t getEarthscopeRequestsCount() const noexcept;

    /// @brief Increments the number of Earthscope retried requests.
    void incrementEarthscopeRetriesCounter() noexcept;
    /// @result The number of Earthscope retried requests. 
    [[nodiscard]] int64_t getEarthscopeRetriesCount() const noexcept;


    ///  @brief Increments the 200 successes for this route.
    void incrementSuccessCounter(const std::string &route);
    /// @result The success counts for the utilized routes.
    [[nodiscard]] std::map<std::string, int64_t> getSuccessCounters() const;

    /// @brief Resets the counters an dutilization.  This is useful for unit tests.
    void resetMetrics() noexcept;
private:
    MetricsSingleton() = default;
    ~MetricsSingleton() = default;
    mutable std::mutex mMutex;
    // The following maps use the form (route, count) (e.g., (auth-login, 1):
    std::map<std::string, int64_t> mServerErrorCounterMap; // 500 response codes
    std::map<std::string, int64_t> mClientErrorCounterMap; // 400 response codes
    std::map<std::string, int64_t> mSuccessCounterMap;     // 200 response codes
    std::atomic<int64_t> mClientUnauthorizedCounter{0};
    std::atomic<int64_t> mClientUnauthenticatedCounter{0};
    std::atomic<int64_t> mBytesDownloadedCounter{0};
    std::atomic<int64_t> mEarthscopeRequestsCounter{0};
    std::atomic<int64_t> mEarthscopeRetriesCounter{0};
};

/// @brief Initializes the metrics singleton once and for all.  This is to be
///        used at application start up.
void initializeMetricsSingleton();
}
#endif
