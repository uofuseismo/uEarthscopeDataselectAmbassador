#include <chrono>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ptree_fwd.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include "uEarthscopeDataselectAmbassador/noDataCacheOptions.hpp"

using namespace UEarthscopeDataselectAmbassador;

namespace
{
constexpr int DEFAULT_TTL_SECONDS{150};
}

class NoDataCacheOptions::NoDataCacheOptionsImpl
{
public:
    std::chrono::milliseconds mTimeToLive
    {
        std::chrono::seconds {DEFAULT_TTL_SECONDS}
    };
    int mMaximumEntries{4096};
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
void NoDataCacheOptions::setMaximumNumberOfEntries(const int maxEntries)
{
    if (maxEntries < 0 || maxEntries > 65535)
    {
        throw std::invalid_argument("Maximum entries must be in [0, 65535]");
    }
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
    if (duration <= std::chrono::seconds {0})
    {
        throw std::invalid_argument("Cached requests TTL must be positive");
    }
    pImpl->mTimeToLive = duration;
}

std::chrono::milliseconds NoDataCacheOptions::getTimeToLive() const noexcept
{
    return pImpl->mTimeToLive;
}

/// Destructor
NoDataCacheOptions::~NoDataCacheOptions() = default;

/// Create from options
NoDataCacheOptions
NoDataCacheOptions::fromInitializationFile(
    const std::filesystem::path &iniFile,
    const std::string &sectionIn)
{
    if (!std::filesystem::exists(iniFile))
    {   
        throw std::invalid_argument(
           std::string {iniFile}.append(" does not exist"));
    }   
    boost::property_tree::ptree propertyTree;
    boost::property_tree::ini_parser::read_ini(iniFile, propertyTree);

    NoDataCacheOptions options;
    auto section = sectionIn;
    if (section.empty()){section = "NoDataCache";}
    if (section.back() != '.'){section.append(".");}
    options.setMaximumNumberOfEntries(
         propertyTree.get<int> (section + "maximumEntries",
                                options.getMaximumNumberOfEntries()));

    auto timeToLiveSeconds
        = propertyTree.get<int> (section + "timeToLiveInSeconds",
                                 DEFAULT_TTL_SECONDS);
    options.setTimeToLive(std::chrono::seconds {timeToLiveSeconds});
    return options;
}
