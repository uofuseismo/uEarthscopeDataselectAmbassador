#include <array>
#include <cstddef>
#include <exception>
#include <ios>
#include <memory>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <curl/curl.h>
#include <spdlog/spdlog.h>
#include <spdlog/logger.h>
//NOLINTNEXTLINE(misc-include-cleaner)
#include <spdlog/sinks/stdout_color_sinks.h>
#include "uEarthscopeDataselectAmbassador/earthscopeClient.hpp"
#include "uEarthscopeDataselectAmbassador/earthscopeClientOptions.hpp"
#include "curl.hpp"

using namespace UEarthscopeDataselectAmbassador;

class EarthscopeClient::EarthscopeClientImpl
{
public:
    EarthscopeClientImpl(const EarthscopeClientOptions &options,
                         const CURLGlobal &curlGlobal,
                         std::shared_ptr<spdlog::logger> logger) :
        mOptions(options),
        mCURLFactory(options, curlGlobal),
        mLogger(std::move(logger))
    {
        if (!mOptions.hasAccessToken() &&
            !mOptions.hasLegacyCredentials())
        {
            throw std::invalid_argument("Client needs a token or credentials");
        }
        if (mLogger == nullptr)
        {   
            // NOLINTBEGIN(misc-include-cleaner)
            constexpr const char *loggerName{"EarthscopeConsole"};
            mLogger = spdlog::get(loggerName);
            if (mLogger == nullptr)
            {   
                mLogger = spdlog::stdout_color_mt(loggerName);
            }   
            // NOLINTEND(misc-include-cleaner)
        }   
        mInitialized = true;   
    }

    EarthscopeClientOptions mOptions;
    CURLFactory mCURLFactory;
    std::shared_ptr<spdlog::logger> mLogger{nullptr};
    bool mInitialized{false};
};

/// Constructor
EarthscopeClient::EarthscopeClient
(
    const EarthscopeClientOptions &options,
    const CURLGlobal &curlGlobal,
    std::shared_ptr<spdlog::logger> logger
) :
    pImpl(std::make_unique<EarthscopeClientImpl> (
            options, curlGlobal, std::move(logger)))
{
}

/// Initialized?
bool EarthscopeClient::isInitialized() const noexcept
{
    return pImpl->mInitialized;
}

/// Destructor
EarthscopeClient::~EarthscopeClient() = default;
