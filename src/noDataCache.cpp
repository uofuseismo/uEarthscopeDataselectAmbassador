#include <chrono>
#include <cstddef>
#include <iterator>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include "uEarthscopeDataselectAmbassador/noDataCache.hpp"
#include "uEarthscopeDataselectAmbassador/noDataCacheOptions.hpp"

using namespace UEarthscopeDataselectAmbassador;

class NoDataCache::NoDataCacheImpl
{
public:
    struct Entry
    {
        std::string key;
        std::chrono::steady_clock::time_point expires;
    };
public:
    NoDataCacheImpl(const NoDataCacheOptions &options) :
        mOptions(options)
    {
        mTimeToLive = mOptions.getTimeToLive();
        mMaximumEntries = mOptions.getMaximumNumberOfEntries();
    }
    
  
    [[nodiscard]] bool contains(const std::string &key,
                                const std::chrono::steady_clock::time_point &now) const
    {
        const std::scoped_lock lock{mMutex};
        evictExpiredLocked(now);
        return mIndex.contains(key);
    }

    void insert(const std::string &key,
                const std::chrono::steady_clock::time_point &now)
    {
        if (mMaximumEntries == 0){return;}
        const std::scoped_lock lock{mMutex};
        // Remove anything expired
        evictExpiredLocked(now);
        // Erase matches
        if (auto it = mIndex.find(key); it != mIndex.end())
        {
            mEntries.erase(it->second);
            mIndex.erase(it);
        }
        // Don't let cache get too big
        if (mEntries.size() >= mMaximumEntries)
        {
            mIndex.erase(mEntries.front().key);
            mEntries.pop_front();
        }
        // Update
        mEntries.push_back(Entry{key, now + mTimeToLive});
        mIndex.emplace(key, std::prev(mEntries.end()));
    }
 
    // Purges old entries
    void evictExpiredLocked(const std::chrono::steady_clock::time_point &now) const
    {
        while (!mEntries.empty() && mEntries.front().expires <= now)
        {
            mIndex.erase(mEntries.front().key);
            mEntries.pop_front();
        }
    }

    // Size
    [[nodiscard]] size_t size() const noexcept
    {
        const std::scoped_lock lock{mMutex};
        return mEntries.size();
    }

    NoDataCacheOptions mOptions;
    mutable std::mutex mMutex;
    mutable std::list<Entry> mEntries;
    mutable std::unordered_map<std::string, std::list<Entry>::iterator> mIndex;
    size_t mMaximumEntries;
    std::chrono::milliseconds mTimeToLive;
};

/// Constructor
NoDataCache::NoDataCache(const NoDataCacheOptions &options) :
    pImpl(std::make_unique<NoDataCacheImpl> (options))
{
}

/// Key in cache? 
bool NoDataCache::contains(
    const std::string &key,
    const std::chrono::steady_clock::time_point &now) const
{
    return pImpl->contains(key, now);
}

/// Insert
void NoDataCache::insert(
    const std::string &key,
    const std::chrono::steady_clock::time_point &now)
{
    pImpl->insert(key, now);
}

/// Size
size_t NoDataCache::size() const noexcept
{
    return pImpl->size();
}

/// Destructor
NoDataCache::~NoDataCache() = default;
