#ifndef UEARTHSCOPE_DATASELECT_AMBASSADOR_NO_DATA_CACHE_OPTIONS_HPP
#define UEARTHSCOPE_DATASELECT_AMBASSADOR_NO_DATA_CACHE_OPTIONS_HPP
#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
namespace UEarthscopeDataselectAmbassador
{
/// @class NoDataCacheOptions noDataCacheOptions.hpp
/// @brief Options defining the no data cache.
/// @copyright Ben Baker (University of Utah) distributed under the
///            MIT NO AI license.
class NoDataCacheOptions
{
public:
    /// @brief Constructor.
    NoDataCacheOptions();
    /// @brief Copy constructor.
    NoDataCacheOptions(const NoDataCacheOptions &options);
    /// @brief Move constructor.
    NoDataCacheOptions(NoDataCacheOptions &&options) noexcept;

    /// @brief Sets the cached request's time to live.
    void setTimeToLive(const std::chrono::milliseconds &requestTimeToLive);
    /// @result The cached request's time to live.
    /// @throw std::invalid_argument if this is not positive.
    /// @note By default this is 150 seconds (2.5 minutes).
    [[nodiscard]] std::chrono::milliseconds getTimeToLive() const noexcept;

    /// @brief Sets the maximum number of entires in the cache.
    /// @note Setting to zero disables the cache.
    void setMaximumNumberOfEntries(uint16_t maximumRetries);
    /// @result The maximum number of entires in the cache.
    /// @note By default this is 4096.
    [[nodiscard]] int getMaximumNumberOfEntries() const noexcept;

    /// @brief Creates the earthscope client options from an
    ///        initialization file.
    [[nodiscard]] static NoDataCacheOptions
        fromInitializationFile(const std::filesystem::path &iniFile,
                               const std::string &section = "NoDataCache");

    /// @brief Destructor.
    ~NoDataCacheOptions();
    /// @brief Copy assignment.
    NoDataCacheOptions& operator=(const NoDataCacheOptions &options);
    /// @brief Move assignment.
    NoDataCacheOptions& operator=(NoDataCacheOptions &&options) noexcept;
private:
    class NoDataCacheOptionsImpl;
    std::unique_ptr<NoDataCacheOptionsImpl> pImpl;
};
}
#endif
