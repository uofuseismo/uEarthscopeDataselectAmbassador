#ifndef UEARTHSCOPE_DATASELECT_AMBASSADOR_NO_DATA_CACHE_HPP
#define UEARTHSCOPE_DATASELECT_AMBASSADOR_NO_DATA_CACHE_HPP
#include <chrono>
#include <memory>

namespace UEarthscopeDataselectAmbassador
{
 class NoDataCacheOptions;
}

namespace UEarthscopeDataselectAmbassador
{
/// @class NoDataCache noDataCacheOptions.hpp
/// @brief This is a way to reject repeat requests quickly and not count
///        against our usage quota.
/// @copyright Ben Baker (University of Utah) distributed under the
///            MIT NO AI license.
class NoDataCache
{
public:
    /// @brief Constructor.
    explicit NoDataCache(const NoDataCacheOptions &options);

    /// @result True indicates the request is in the cache.
    [[nodiscard]] bool contains(const std::string &key,
                                const std::chrono::steady_clock::time_point &now = std::chrono::steady_clock::now()) const; 
    /// @brief Inserts the request into the cache.
    void insert(const std::string &key,
                const std::chrono::steady_clock::time_point &now = std::chrono::steady_clock::now());

    /// @result The current cache size.
    [[nodiscard]] size_t size() const noexcept;

    /// @brief Destructor.
    ~NoDataCache();

    NoDataCache() = delete;
    NoDataCache(const NoDataCache &) = delete;
    NoDataCache(NoDataCache &&) noexcept = delete;
    NoDataCache& operator=(const NoDataCache &) = delete;
    NoDataCache& operator=(NoDataCache &&) noexcept = delete;
private:
    class NoDataCacheImpl;
    std::unique_ptr<NoDataCacheImpl> pImpl;
};

}
#endif
