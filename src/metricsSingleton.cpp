#include <chrono>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include "uEarthscopeDataselectAmbassador/metricsSingleton.hpp"

using namespace UEarthscopeDataselectAmbassador::Metrics;

MetricsSingleton &MetricsSingleton::getInstance()
{
    static MetricsSingleton instance;
    return instance;
}

/// Server error counts 
void MetricsSingleton::incrementServerErrorCounter(
    const std::string &route)
{
    if (route.empty()){return;}
    {
    const std::scoped_lock lock{mMutex};
    auto it = mServerErrorCounterMap.find(route);
    if (it != mServerErrorCounterMap.end())
    {
        it->second = it->second + 1;
        return;
    }
    mServerErrorCounterMap.insert( {route, 1} );
    }
}

std::map<std::string, int64_t> MetricsSingleton::getServerErrorCounters() const
{
    const std::scoped_lock lock{mMutex};
    return mServerErrorCounterMap;
}

/// Client error counts 
void MetricsSingleton::incrementClientErrorCounter(
    const std::string &route)
{
    if (route.empty()){return;}
    {
    const std::scoped_lock lock{mMutex};
    auto it = mClientErrorCounterMap.find(route);
    if (it != mClientErrorCounterMap.end())
    {
        it->second = it->second + 1;
        return;
    }
    mClientErrorCounterMap.insert( {route, 1} );
    }
}

std::map<std::string, int64_t> MetricsSingleton::getClientErrorCounters() const
{
    const std::scoped_lock lock{mMutex};
    return mClientErrorCounterMap;
}

/// Success counts 
void MetricsSingleton::incrementSuccessCounter(
    const std::string &route)
{
    if (route.empty()){return;}
    {   
    const std::scoped_lock lock{mMutex};
    auto it = mSuccessCounterMap.find(route);
    if (it != mSuccessCounterMap.end())
    {   
        it->second = it->second + 1;
        return;
    }   
    mSuccessCounterMap.insert( {route, 1} );
    }   
}

std::map<std::string, int64_t> MetricsSingleton::getSuccessCounters() const
{
    const std::scoped_lock lock{mMutex};
    return mSuccessCounterMap;
}

/// Unauthorized 
void MetricsSingleton::incrementClientUnauthorizedCounter() noexcept
{
    mClientUnauthorizedCounter.fetch_add(1, std::memory_order_relaxed);        
}

int64_t MetricsSingleton::getClientUnauthorizedCounts() const noexcept
{
    return mClientUnauthorizedCounter.load(std::memory_order_relaxed);
}

/*
/// Unauthenticated
void MetricsSingleton::incrementUnauthenticatedCounter() noexcept
{
    mUnauthenticatedCounter.fetch_add(1, std::memory_order_relaxed);
}

int64_t MetricsSingleton::getUnauthenticatedCounts() const noexcept
{
    return mUnauthenticatedCounter.load(std::memory_order_relaxed);
}
*/

/// Number of bytes downloaded from Earthscope
void MetricsSingleton::incrementBytesDownloadedCounter(const uint32_t nBytes)
{
    mBytesDownloadedCounter.fetch_add(nBytes, std::memory_order_relaxed);
}

int64_t MetricsSingleton::getBytesDownloadedCount() const noexcept
{
    return mBytesDownloadedCounter.load(std::memory_order_relaxed);
}

// Earthscope request counter
void MetricsSingleton::incrementEarthscopeRequestsCounter() noexcept
{
    mEarthscopeRequestsCounter.fetch_add(1, std::memory_order_relaxed);
}

int64_t MetricsSingleton::getEarthscopeRequestsCount() const noexcept
{
    return mEarthscopeRequestsCounter.load(std::memory_order_relaxed);
}

// Earthscope retry counter
void MetricsSingleton::incrementEarthscopeRetriesCounter() noexcept
{
    mEarthscopeRetriesCounter.fetch_add(1, std::memory_order_relaxed);
}

int64_t MetricsSingleton::getEarthscopeRetriesCount() const noexcept
{
    return mEarthscopeRetriesCounter.load(std::memory_order_relaxed);
}

/// Rate limits
void MetricsSingleton::incrementRateLimitedRequests() noexcept
{
    mRateLimitedRequestsCounter.fetch_add(1, std::memory_order_relaxed);     
}

int64_t MetricsSingleton::getRateLimitedRequestsCount() const noexcept
{
    return mRateLimitedRequestsCounter.load(std::memory_order_relaxed);
}


/// Reset
void MetricsSingleton::resetMetrics() noexcept
{
    const std::scoped_lock lock{mMutex};
    mClientUnauthorizedCounter.store(0);
    mClientUnauthenticatedCounter.store(0);
    mBytesDownloadedCounter.store(0);
    mEarthscopeRequestsCounter.store(0);
    mEarthscopeRetriesCounter.store(0);
    mRateLimitedRequestsCounter.store(0);
    for (auto &counter : mServerErrorCounterMap)
    {
        counter.second = 0;
    }
    for (auto &counter : mClientErrorCounterMap)
    {
        counter.second = 0;
    }
    for (auto &counter : mSuccessCounterMap)
    {
        counter.second = 0;
    }
}

/// Initialize
void UEarthscopeDataselectAmbassador::Metrics::initializeMetricsSingleton()
{
    MetricsSingleton::getInstance();
}
