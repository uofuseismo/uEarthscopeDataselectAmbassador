#include <cstddef>
#include <exception>
#include <expected>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <spdlog/spdlog.h>
#include <spdlog/logger.h>
//NOLINTNEXTLINE(misc-include-cleaner)
#include <spdlog/sinks/stdout_color_sinks.h>
#include "uEarthscopeDataselectAmbassador/earthscopeClient.hpp"
#include "uEarthscopeDataselectAmbassador/earthscopeClientOptions.hpp"
#include "uEarthscopeDataselectAmbassador/metricsSingleton.hpp"
#include "uEarthscopeDataselectAmbassador/request.hpp"
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
        mEndpoint = mOptions.getEndpoint();
        if (mEndpoint.back() != '/'){mEndpoint.push_back('/');}
        // Figure out the auth mode
        if (mOptions.hasAccessToken())
        { 
            mEndpoint.append("query?");
            mUseToken = true;
        }
        else if (options.hasLegacyCredentials())
        {
            mEndpoint.append("queryauth?");
            mUseToken = false;
        }
        else
        {
            throw std::invalid_argument(
               "Must set either access token or legacy credentials");
        }
        mInitialized = true;   
    }

    /// GET Request
    [[nodiscard]]
    std::expected
    <
        EarthscopeClient::Response,
        EarthscopeClient::Error
    > get(const std::string &url) const
    {
        if (!mInitialized)
        {
            SPDLOG_LOGGER_ERROR(mLogger,
                                "Earthscope client not initialized");
            return std::unexpected(Error::ServerError);
        }
        if (url.empty())
        {
            return std::unexpected(Error::EmptyURL);
        } 
        // Create the curl handle
        std::unique_ptr<CURLHandle> curlHandle{nullptr}; 
        try 
        {   
            curlHandle = mCURLFactory.create(mOptions, mUseToken);
        }
        catch (const std::exception &e) 
        {
            SPDLOG_LOGGER_ERROR(mLogger,
                                "Failed to create curl handle; failed with {}",
                                e.what());
            return std::unexpected(Error::ServerError);
        }
        auto &metrics
            = UEarthscopeDataselectAmbassador::Metrics::
              MetricsSingleton::getInstance();
        // GET request and classify the output
        try
        {
            metrics.incrementEarthscopeRequestsCounter();
            auto curlResponse = curlHandle->get(url, mLogger.get());
            if (curlResponse.has_value())
            {
                Response response
                {
                    std::move(curlResponse->body),
                    std::move(curlResponse->contentType),
                    static_cast<int> (curlResponse->status)
                };
                metrics.incrementBytesDownloadedCounter(
                    response.payload.size());
                return std::move(response);
            }
            else
            {
                if (curlResponse.error() == CURLHandle::Error::InvalidURL)
                {
                    return std::unexpected(Error::InvalidRequest);
                }
                else if (curlResponse.error() ==
                         CURLHandle::Error::InvalidCURLOption)
                {
                    return std::unexpected(Error::ServerError);
                }
                else if (curlResponse.error() ==
                         CURLHandle::Error::Uninitialized)
                {
                    return std::unexpected(Error::ServerError);
                }
                else if (curlResponse.error() == CURLHandle::Error::TimedOut)
                {
                    return std::unexpected(Error::TimedOut);
                }
                else if (curlResponse.error() ==
                         CURLHandle::Error::ConnectionFailed)
                {
                    return std::unexpected(Error::ConnectionFailure);
                }
                else if (curlResponse.error() == CURLHandle::Error::TLSFailed)
                {
                    return std::unexpected(Error::ServerError);
                }
                else if (curlResponse.error() == CURLHandle::Error::TooLarge)
                {
                    return std::unexpected(Error::TooLarge);
                }
                else if (curlResponse.error() == CURLHandle::Error::Other)
                {
                    return std::unexpected(Error::ServerError);
                }
                else
                {
                    SPDLOG_LOGGER_ERROR(mLogger, "Unhandled cURL error");
                }
                return std::unexpected(Error::ServerError);
            }
        }
        catch (const std::exception &e)
        {
            SPDLOG_LOGGER_ERROR(mLogger,
                                "Failed to issued curl request; failed with {}",
                                e.what());
            return std::unexpected(Error::ServerError);
        }
    }

    EarthscopeClientOptions mOptions;
    CURLFactory mCURLFactory;
    std::shared_ptr<spdlog::logger> mLogger{nullptr};
    std::string mEndpoint;
    bool mInitialized{false};
    bool mUseToken{true};
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

/*
/// Request
std::expected<EarthscopeClient::Response, EarthscopeClient::Error>
EarthscopeClient::get(const std::string &url) const
{
    if (!isInitialized())
    {   
        SPDLOG_LOGGER_ERROR(pImpl->mLogger,
                            "Earthscope client not initialized");
        return std::unexpected(Error::ServerError);
    }    
    if (url.empty())
    {
        return std::unexpected(Error::EmptyURL);
    }
    std::unique_ptr<CURLHandle> curlHandle{nullptr}; 
    try
    {
        curlHandle
           = pImpl->mCURLFactory.create(pImpl->mOptions, pImpl->mUseToken);
    }
    catch (const std::exception &e)
    {
        SPDLOG_LOGGER_ERROR(pImpl->mLogger,
                            "Failed to create curl handle; failed with {}",
                            e.what());
        return std::unexpected(Error::ServerError);
    }
    try
    {
        auto curlResponse = curlHandle->get(url, pImpl->mLogger.get());
        if (curlResponse.has_value())
        {   
            Response response
            {
                std::move(curlResponse->body),
                std::move(curlResponse->contentType),
                static_cast<int> (curlResponse->status)
            };
            return std::move(response);
        }
        else
        {
            if (curlResponse.error() == CURLHandle::Error::InvalidURL)
            {
                return std::unexpected(EarthscopeClient::Error::InvalidRequest);
            }
            else if (curlResponse.error() ==
                     CURLHandle::Error::InvalidCURLOption)
            {
                return std::unexpected(EarthscopeClient::Error::ServerError); 
            }
            else if (curlResponse.error() ==
                     CURLHandle::Error::Uninitialized)
            {
                return std::unexpected(EarthscopeClient::Error::ServerError);
            }
            else if (curlResponse.error() == CURLHandle::Error::TimedOut)
            {
                return std::unexpected(EarthscopeClient::Error::TimedOut);
            }
            else if (curlResponse.error() ==
                     CURLHandle::Error::ConnectionFailed)
            {
                return std::unexpected(Error::ConnectionFailure);
            }
            else if (curlResponse.error() == CURLHandle::Error::TLSFailed)
            {
                return std::unexpected(EarthscopeClient::Error::ServerError);
            }
            else if (curlResponse.error() == CURLHandle::Error::TooLarge)
            {
                return std::unexpected(EarthscopeClient::Error::TooLarge);
            }
            else if (curlResponse.error() == CURLHandle::Error::Other)
            {
                return std::unexpected(EarthscopeClient::Error::ServerError);
            }
            else
            {
                SPDLOG_LOGGER_ERROR(pImpl->mLogger, "Unhandled cURL error");
            }
            return std::unexpected(EarthscopeClient::Error::ServerError);
        }
    }
    catch (const std::exception &e)
    {
        SPDLOG_LOGGER_ERROR(pImpl->mLogger,
                            "Failed to issued curl request; failed with {}",
                            e.what());
        return std::unexpected(Error::ServerError);
    }
}
*/

/// Request
std::expected<EarthscopeClient::Response, EarthscopeClient::Error>
EarthscopeClient::get(const Request &request)  const
{
    if (!isInitialized())
    {
        SPDLOG_LOGGER_ERROR(pImpl->mLogger,
                            "Earthscope client not initialized");
        return std::unexpected(Error::ServerError);
    }    
    std::string url{pImpl->mEndpoint};
    try
    {
        url.append(request.toString());
    }
    catch (const std::exception &e)
    {
        SPDLOG_LOGGER_WARN(pImpl->mLogger,
                           "Could not generate request URL because {}",
                           e.what());
        return std::unexpected(Error::InvalidRequest);
    }
    return pImpl->get(url);
}

/// Destructor
EarthscopeClient::~EarthscopeClient() = default;
