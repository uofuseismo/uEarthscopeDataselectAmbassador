#include <exception>
#include <expected>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#ifndef NDEBUG
#include <cassert>
#endif
#include <spdlog/spdlog.h>
#include <spdlog/logger.h>
//NOLINTNEXTLINE(misc-include-cleaner)
#include <spdlog/sinks/stdout_color_sinks.h>
#include "uEarthscopeDataselectAmbassador/bulkRequest.hpp"
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
        mCURLFactory(mOptions, curlGlobal),
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
#ifndef NDEBUG
        assert(mEndpoint.back() != '?');
#endif
        if (mEndpoint.back() != '/'){mEndpoint.push_back('/');}
        // Figure out the auth mode
        if (mOptions.hasAccessToken())
        { 
            mEndpoint.append("query");
            mUseToken = true;
        }
        else if (options.hasLegacyCredentials())
        {
            mEndpoint.append("queryauth");
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
        auto &metrics
            = UEarthscopeDataselectAmbassador::Metrics::
              MetricsSingleton::getInstance();
        // Create the curl handle
        std::unique_ptr<CURLHandle> curlHandle{nullptr}; 
        try
        {
            constexpr bool isGet{true};
            curlHandle = mCURLFactory.create(mOptions, mUseToken, isGet);
        }
        catch (const std::exception &e)
        {
            SPDLOG_LOGGER_ERROR(mLogger,
                "Failed to create curl GET handle; failed with {}",
                e.what());
            return std::unexpected(Error::ServerError);
        }
        // Handle retries
        constexpr int maximumAttempts{2};
        for (int attempt = 1; attempt <= maximumAttempts; ++attempt)
        {
            // GET request and classify the output
            try
            {
                metrics.incrementEarthscopeRequestsCounter();
                if (attempt > 1){metrics.incrementEarthscopeRetriesCounter();}
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
                    // Quick check on timeout and retry
                    if (curlResponse.error() == CURLHandle::Error::TimedOut &&
                        attempt < maximumAttempts)
                    {
                        SPDLOG_LOGGER_WARN(
                            mLogger,
                            "Earthscope GET request timed out; will retry");
                        continue;
                    }
                    // Okay handle the error
                    return std::unexpected(checkError(curlResponse.error())); 
                }
            }
            catch (const std::exception &e)
            {
                SPDLOG_LOGGER_ERROR(mLogger,
                    "Failed to issue curl GET request; failed with {}",
                    e.what());
                return std::unexpected(Error::ServerError);
            }
        } // Loop on retries
        SPDLOG_LOGGER_ERROR(mLogger, "Failed to handle GET return logic");
        return std::unexpected(Error::ServerError);
    }

    /// GET Request
    [[nodiscard]]
    std::expected
    <
        EarthscopeClient::Response,
        EarthscopeClient::Error
    > 
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    post(const std::string &url, const std::string &postBody) const
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
        auto &metrics
            = UEarthscopeDataselectAmbassador::Metrics::
              MetricsSingleton::getInstance();
        // Create the curl handle
        std::unique_ptr<CURLHandle> curlHandle{nullptr};
        try
        {
            constexpr bool isGet{false};
            curlHandle = mCURLFactory.create(mOptions, mUseToken, isGet);
        }
        catch (const std::exception &e)
        {
            SPDLOG_LOGGER_ERROR(mLogger,
                "Failed to create curl POST handle; failed with {}",
                e.what());
            return std::unexpected(Error::ServerError);
        }
        // Handle retries
        constexpr int maximumAttempts{2};
        for (int attempt = 1; attempt <= maximumAttempts; ++attempt)
        {
            // POST request and classify the output
            try
            {
                metrics.incrementEarthscopeRequestsCounter();
                if (attempt > 1){metrics.incrementEarthscopeRetriesCounter();}
                auto curlResponse
                    = curlHandle->post(url, postBody, mLogger.get());
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
                    // Quick check on timeout and retry
                    if (curlResponse.error() == CURLHandle::Error::TimedOut &&
                        attempt < maximumAttempts)
                    {
                        SPDLOG_LOGGER_WARN(
                            mLogger,
                            "Earthscope POST request timed out; will retry");
                        continue;
                    }
                    // Okay handle the error
                    return std::unexpected(checkError(curlResponse.error()));
                }
            }
            catch (const std::exception &e)
            {
                SPDLOG_LOGGER_ERROR(mLogger,
                    "Failed to issue curl POST request; failed with {}",
                    e.what());
                return std::unexpected(Error::ServerError);
            }
        }
        SPDLOG_LOGGER_ERROR(mLogger, "Failed to handle POST return logic");
        return std::unexpected(Error::ServerError);
    }

    [[nodiscard]] Error checkError(const CURLHandle::Error error) const noexcept
    {
        if (error == CURLHandle::Error::InvalidURL)
        {
            return Error::InvalidRequest;
        }
        else if (error == CURLHandle::Error::EmptyPOSTBody)
        {
            return Error::InvalidRequest;
        }
        else if (error == CURLHandle::Error::InvalidCURLOption)
        {
            return Error::ServerError;
        }
        else if (error == CURLHandle::Error::Uninitialized)
        {
            return Error::ServerError;
        }
        else if (error == CURLHandle::Error::TimedOut)
        {
            return Error::TimedOut;
        }
        else if (error == CURLHandle::Error::ConnectionFailed)
        {
            return Error::ConnectionFailure;
        }
        else if (error == CURLHandle::Error::TLSFailed)
        {
            return Error::ServerError;
        }
        else if (error == CURLHandle::Error::TooLarge)
        {
            return Error::TooLarge;
        }
        else if (error == CURLHandle::Error::Other)
        {
            return Error::ServerError;
        }
        else
        {
            SPDLOG_LOGGER_ERROR(mLogger, "Unhandled cURL error");
        }
        return Error::ServerError;
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

/// GET request
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
    url.append("?");
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

/// POST request
std::expected<EarthscopeClient::Response, EarthscopeClient::Error>
EarthscopeClient::post(const BulkRequest &request) const
{
    if (!isInitialized())
    {
        SPDLOG_LOGGER_ERROR(pImpl->mLogger,
                            "Earthscope client not initialized");
        return std::unexpected(Error::ServerError);
    }
    if (request.empty())
    {
        SPDLOG_LOGGER_WARN(pImpl->mLogger,
                           "No requests is bulk request");
        return std::unexpected(Error::InvalidRequest);
    }
    const std::string url{pImpl->mEndpoint};
    std::string postBody;
    try
    {
        postBody = request.toString();
    }
    catch (const std::exception &e)
    {
        SPDLOG_LOGGER_ERROR(pImpl->mLogger,
                            "Could not generate post body because {}",
                            e.what());
        return std::unexpected(Error::ServerError);
    }
    return pImpl->post(url, postBody);
}

/// Destructor
EarthscopeClient::~EarthscopeClient() = default;
