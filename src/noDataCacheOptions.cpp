#include <chrono>
#include <cstdint>
#include <memory>
#include <utility>
#include "uEarthscopeDataselectAmbassador/noDataCacheOptions.hpp"

using namespace UEarthscopeDataselectAmbassador;

class NoDataCacheOptions::NoDataCacheOptionsImpl
{
public:
    std::chrono::milliseconds mTimeToLive{std::chrono::seconds {120}};
    int mMaximumEntries{2048};     
};

/// Constructor
NoDataCacheOptions::NoDataCacheOptions() :
    pImpl(std::make_unique<NoDataCacheOptionsImpl> ())
{
}

/// Copy constructor
NoDataCacheOptions::NoDataCacheOptions(const NoDataCacheOptions &options)
{
    *this = options;
}

/// Move constructor
NoDataCacheOptions::NoDataCacheOptions(NoDataCacheOptions &&options) noexcept
{
    *this = std::move(options);
}

/// Copy assignment
NoDataCacheOptions& 
NoDataCacheOptions::operator=(const NoDataCacheOptions &options)
{
    if (&options == this){return *this;}
    pImpl = std::make_unique<NoDataCacheOptionsImpl> (*options.pImpl);
    return *this;
}

/// Move assignment
NoDataCacheOptions& 
NoDataCacheOptions::operator=(NoDataCacheOptions &&options) noexcept
{
    if (&options == this){return *this;}
    pImpl = std::move(options.pImpl);
    return *this;
}

/// Maximum number of entries
void NoDataCacheOptions::setMaximumNumberOfEntries(const uint16_t maxEntries)
{
    pImpl->mMaximumEntries = static_cast<int> (maxEntries);
}

int NoDataCacheOptions::getMaximumNumberOfEntries() const noexcept
{
    return pImpl->mMaximumEntries;
} 

/// Time to live
void NoDataCacheOptions::setTimeToLive(
    const std::chrono::milliseconds &duration)
{
    pImpl->mTimeToLive = duration;
}

std::chrono::milliseconds NoDataCacheOptions::getTimeToLive() const noexcept
{
    return pImpl->mTimeToLive;
}

/// Destructor
NoDataCacheOptions::~NoDataCacheOptions() = default;
