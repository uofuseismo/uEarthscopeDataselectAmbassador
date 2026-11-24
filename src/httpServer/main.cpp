#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <exception>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <stdlib.h> // setenv
#include <spdlog/spdlog.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h> //NOLINT
#include <crow/app.h>
#include <crow/common.h>
#include <crow/http_request.h>
#include <crow/http_response.h>
#include <crow/logging.h>
#include "uEarthscopeDataselectAmbassador/bulkRequest.hpp"
#include "uEarthscopeDataselectAmbassador/curlGlobal.hpp"
#include "uEarthscopeDataselectAmbassador/earthscopeClient.hpp"
//NOLINTNEXTLINE(misc-include-cleaner)
#include "uEarthscopeDataselectAmbassador/earthscopeClientOptions.hpp"
#include "uEarthscopeDataselectAmbassador/metricsSingleton.hpp"
#include "uEarthscopeDataselectAmbassador/noDataCache.hpp"
#include "uEarthscopeDataselectAmbassador/rateLimiter.hpp"
#include "uEarthscopeDataselectAmbassador/request.hpp"
#include "uEarthscopeDataselectAmbassador/version.hpp"
#include "utilities/logger.hpp"
#include "httpServer/metrics.hpp"
#include "httpServer/parseCommandLineOptions.hpp"
#include "httpServer/programOptions.hpp"
#include "httpServer/crowHelper.hpp"

/*
namespace
{
[[nodiscard]] crow::response toErrorResponse(const int status,
                                             const std::string &reason,
                                             const std::string &details)
{
    crow::response response{status,
        std::format("Error {}: {}\n\n{}\n", status, reason, details)};
    response.set_header("Content-Type", "text/plain");
    return response;
}

[[nodiscard]] crow::response toCrowResponse(
    std::expected<UEarthscopeDataselectAmbassador::EarthscopeClient::Response,
                  UEarthscopeDataselectAmbassador::EarthscopeClient::Error> &&result,
    spdlog::logger &logger)
{
    using Error = UEarthscopeDataselectAmbassador::EarthscopeClient::Error;
    if (!result.has_value())
    {
        if (result.error() == Error::TimedOut)
        {
            return ::toErrorResponse(504,
                                     "Gateway Timeout",
                                     "EarthScope did not respond in time");
        }
        else if (result.error() == Error::ConnectionFailure)
        {
            return ::toErrorResponse(502,
                                     "Bad Gateway",
                                     "Could not reach EarthScope");
        }
        else if (result.error() == Error::TooLarge)
        {
            return ::toErrorResponse(413,
                                     "Payload Too Large",
                                     "Request a shorter time window");
        }
        else if (result.error() == Error::InvalidRequest)
        {
            return ::toErrorResponse(400,
                                     "Bad Request",
                                     "Invalid request");
        }
        // This should never happen - shouldn't give same request either
        // is 500 right?
        else if (result.error() == Error::EmptyURL) 
        {
            return ::toErrorResponse(500,
                                     "Internal Server Error",
                                     "Internal error");
        }
        return ::toErrorResponse(500,
                                 "Internal Server Error",
                                 "Internal error");
    }

    auto &upstream = *result;
    if (upstream.status == 200)
    {
        crow::response response{200, std::move(upstream.payload)};
        response.set_header("Content-Type",
                            upstream.contentType.empty() ?
                            "application/vnd.fdsn.mseed" : upstream.contentType);
        return response;
    }
    else if (upstream.status == 204)
    {
        return crow::response{204};
    }
    else if (upstream.status == 400 ||
             upstream.status == 404 ||
             upstream.status == 413)
    {
        return ::toErrorResponse(upstream.status,
                                 upstream.status == 404 ? "Not Found" :
                                 upstream.status == 413 ? "Payload Too Large" : "Bad Request",
                                 upstream.payload);
    }
    else if (upstream.status == 401 || upstream.status == 403)
    {
        SPDLOG_LOGGER_CRITICAL(&logger,
                               "EarthScope rejected our credentials ({})",
                               upstream.status);
        return ::toErrorResponse(502,
                                 "Bad Gateway",
                                 "Upstream authentication failed");
    }
    else if (upstream.status == 429)
    {
        SPDLOG_LOGGER_CRITICAL(&logger, "EarthScope quota exceeded");
        return ::toErrorResponse(503,
                                 "Service unavailable",
                                 "Upstream quota exceeded");
    }
    SPDLOG_LOGGER_WARN(&logger,
                       "Unexpected EarthScope status {}",
                        upstream.status);
    if (upstream.status >= 500)
    {
        return ::toErrorResponse(503,
                                 "Service Unavailable",
                                 "EarthScope is unavailable");
    }
    return ::toErrorResponse(502,
                             "Bad Gateway",
                             "Unexpected response from EarthScope");
}

}
*/

class CustomLogger : public crow::ILogHandler
{
public:
    CustomLogger()
    {
        ::ProgramOptions programOptions;
        programOptions.exportLogs = false;
        programOptions.verbosity = 3;
        logger
            = UEarthscopeDataselectAmbassador::Logger::initialize(
                programOptions);
        if (logger == nullptr)
        {
            throw std::runtime_error(
                "Failed to initialize crow console logger");
        }
    }
    explicit CustomLogger(std::shared_ptr<spdlog::logger> existingLogger) :
        logger(std::move(existingLogger))
    {   
        if (logger == nullptr)
        {
            // NOLINTBEGIN(misc-include-cleaner)
             constexpr const char *loggerName{"crow-console"};
            // N.B. stdout_color_mt throws if the name is already registered.
            auto logger = spdlog::get(loggerName);
            if (logger == nullptr)
            {
                logger = spdlog::stdout_color_mt(loggerName);
            }
            // NOLINTEND(misc-include-cleaner)
        }
        if (logger == nullptr)
        {
            throw std::runtime_error("Failed to initialize logger");
        }
    }
    ~CustomLogger()
    {
        logger = nullptr;
    }
    void log(const std::string &message,  crow::LogLevel level)
    {
        if (logger == nullptr){return;}
        // Most common
        if (level == crow::LogLevel::Info)
        {
            logger->info(message);
        }
        else if (level == crow::LogLevel::Warning)
        {
            logger->warn(message);
        }
        else if (level == crow::LogLevel::Error)
        {
            logger->error(message);
        }
        else if (level == crow::LogLevel::Critical)
        {
            logger->critical(message);
        }
        else if (level == crow::LogLevel::Debug)
        {
            logger->debug(message);
        }
        else
        {
            logger->warn("Unhandled log level - logging " + message);
        }
    }
    std::shared_ptr<spdlog::logger> logger{nullptr};
};

int main(int argc, char *argv[])
{
    using namespace UEarthscopeDataselectAmbassador;
    const CURLGlobal curlGlobal;
    Metrics::initializeMetricsSingleton();
    //NOLINTNEXTLINE(misc-include-cleaner)
    auto consoleLogger = spdlog::stdout_color_st("console");
    SPDLOG_LOGGER_INFO(consoleLogger,
                       "Running version {} of uEarthscopeDataselectAmbassador",
                       Version::getVersionWithTag());

    // Parse the command line arguments
    std::filesystem::path iniFile;
    try
    {
        auto [iniFileName, isHelp] = ::parseCommandLineOptions(argc, argv);
        if (isHelp){return EXIT_SUCCESS;}
        if (iniFileName.empty())
        {
            throw std::runtime_error("No initialization file specified");
        }
        iniFile = iniFileName;
    }
    catch (const std::exception &e)
    {
        SPDLOG_LOGGER_CRITICAL(consoleLogger,
                               "Failed to read command line options because {}",
                               std::string {e.what()});
        return EXIT_FAILURE;
    }

    // Parse the ini file
    ::ProgramOptions programOptions;
    try
    {
        programOptions.parseInitializationFile(iniFile);
    }
    catch (const std::exception &e)
    {
        SPDLOG_LOGGER_CRITICAL(consoleLogger,
                               "Failed to read program options because {}",
                               std::string {e.what()});
        return EXIT_FAILURE;
    }
    if (getenv("OTEL_SERVICE_NAME") == nullptr)
    {
        constexpr int overwrite{1};
        setenv("OTEL_SERVICE_NAME",
               programOptions.applicationName.c_str(),
               overwrite);
    }
    // Create the logger (and one for crow)
    std::shared_ptr<spdlog::logger> logger{Logger::initialize(programOptions)};
    ::CustomLogger customLogger{logger};

    // Initialize the metrics
    try 
    {
        Metrics::initialize(programOptions);
        Metrics::createMeters(programOptions);
    }
    catch (const std::exception &e) 
    {
        SPDLOG_LOGGER_CRITICAL(logger,
                               "Failed to initialize metrics because {}",
                               std::string {e.what()});
        return EXIT_FAILURE;
    }

    // Create the star of the show
    EarthscopeClient
        earthscopeClient{programOptions.earthscopeClientOptions,
                         curlGlobal,
                         logger};

    // Request rate limiter
    RateLimiter rateLimiter{programOptions.maximumRequestsPerSecond};

    // Limiter to back off on 204 errors and not crush our request quota
    std::unique_ptr<NoDataCache> noDataCache{nullptr};
    if (programOptions.noDataCacheOptions.getMaximumNumberOfEntries() > 0)
    {
        noDataCache
            = std::make_unique<NoDataCache> (programOptions.noDataCacheOptions);
    }

    crow::logger::setHandler(&customLogger);
    crow::SimpleApp app;
    CROW_ROUTE(app, "/")
    ([&]() -> crow::response
    {   
        SPDLOG_LOGGER_DEBUG(customLogger.logger, "Default route");
        return crow::response(200);
    });
    CROW_ROUTE(app, "/fdsnws/dataselect/1/query")
        .methods(crow::HTTPMethod::Get)
    ([&](const crow::request &httpRequest) -> crow::response
    {
        auto &metrics = Metrics::MetricsSingleton::getInstance();
        const auto startClock = std::chrono::steady_clock::now();
        SPDLOG_LOGGER_DEBUG(logger, "GET query starting");
        const auto mark = httpRequest.raw_url.find('?');
        const std::string query
        {
            mark == std::string::npos ?
                    std::string {} : httpRequest.raw_url.substr(mark + 1)
        };
        std::optional<Request> request;
        try
        {
            request.emplace(query);
        }
        catch (const std::invalid_argument &e)
        {
            SPDLOG_LOGGER_DEBUG(logger, "Rejected query: {}", e.what());
            return ::toErrorResponse(400, "Bad Request", e.what());
        }
        catch (const std::exception &e)
        {
            SPDLOG_LOGGER_ERROR(logger, "Failed to parse query: {}", e.what());
            return ::toErrorResponse(500, "Internal Server Error", "Internal error");
        }
        // In the cache?
        std::string cacheKey;
        if (noDataCache != nullptr)
        {
            try
            {
                cacheKey = request->toString();
                if (noDataCache->contains(cacheKey))
                {
                    metrics.increment204CacheHitCounter();
                    SPDLOG_LOGGER_DEBUG(logger,
                        "Cache hit {}.{}.{}.{}", 
                        request->getNetwork().value_or("*"),
                        request->getStation().value_or("*"),
                        request->getLocationCode().value_or("*"),
                        request->getChannel().value_or("*"));
                    return crow::response{204};
                }
            }
            catch (const std::exception &e)
            {
                SPDLOG_LOGGER_WARN(logger, "Failed to check cache because {}",
                                   e.what());
            }
        }
        if (rateLimiter.rateExceeded())
        {
            metrics.incrementRateLimitedRequests();
            SPDLOG_LOGGER_DEBUG(logger,
                                "Rate limited {}.{}.{}.{}",
                                request->getNetwork().value_or("*"),
                                request->getStation().value_or("*"),
                                request->getLocationCode().value_or("*"),
                                request->getChannel().value_or("*"));
            auto response = ::toErrorResponse(503,
                                              "Service Unavailable",
                                              "Rate limit exceeded");
            response.set_header("Retry-After", "1");
            return response;
        }
        SPDLOG_LOGGER_DEBUG(
            logger,
            "Requesting {}.{}.{}.{}",
            request->getNetwork().value_or("*"),
            request->getStation().value_or("*"),
            request->getLocationCode().value_or("*"),
            request->getChannel().value_or("*"));
        //const auto startClock = std::chrono::steady_clock::now();
        auto response = ::toCrowResponse(earthscopeClient.get(*request), *logger);
        if (response.code == 200)
        {
            auto elapsedTime = std::chrono::steady_clock::now() - startClock;
            Metrics::recordClientRequestDuration(
                "get",
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                   elapsedTime));
            metrics.incrementSuccessCounter("get");
            SPDLOG_LOGGER_INFO(
                logger,
                "Successful Earthscope query for {}.{}.{}.{} -> {} ({} bytes, {} ms)",
                request->getNetwork().value_or("*"),
                request->getStation().value_or("*"),
                request->getLocationCode().value_or("*"),
                request->getChannel().value_or("*"),
                response.code, response.body.size(),
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    elapsedTime).count());
        }
        else if (response.code == 204)
        {
            if (noDataCache != nullptr)
            {
                try
                {
                    noDataCache->insert(cacheKey);
                }
                catch (const std::exception &e)
                {
                    SPDLOG_LOGGER_WARN(logger,
                                       "Failed to update cache because {}",
                                       e.what() );
                }
            }
            auto elapsedTime = std::chrono::steady_clock::now() - startClock;
            Metrics::recordClientRequestDuration(
                "get",
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                   elapsedTime));
            SPDLOG_LOGGER_INFO(
                logger,
                "Earthscope query generated no data for {}.{}.{}.{} -> ({} ms)",
                request->getNetwork().value_or("*"),
                request->getStation().value_or("*"),
                request->getLocationCode().value_or("*"),
                request->getChannel().value_or("*"),
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    elapsedTime).count());
        }
        else
        {
            auto elapsedTime = std::chrono::steady_clock::now() - startClock;
            Metrics::recordClientRequestDuration(
                "get",
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                   elapsedTime));
            SPDLOG_LOGGER_WARN(
                logger,
                "GET request for {}.{}.{}.{} failed",
                request->getNetwork().value_or("*"),
                request->getStation().value_or("*"),
                request->getLocationCode().value_or("*"),
                request->getChannel().value_or("*"));
        }
        return response;
    });
    CROW_ROUTE(app, "/fdsnws/dataselect/1/query")
        .methods(crow::HTTPMethod::Post)
    ([&](const crow::request &httpRequest) -> crow::response
    {   
        auto &metrics = Metrics::MetricsSingleton::getInstance();
        const auto startClock = std::chrono::steady_clock::now();
        SPDLOG_LOGGER_DEBUG(logger, "POST query starting");

        std::optional<BulkRequest> bulkRequest;
        try
        {
            bulkRequest.emplace(httpRequest.body);
        }
        catch (const std::invalid_argument &e)
        {
            SPDLOG_LOGGER_DEBUG(logger, "Rejected query: {}", e.what());
            return ::toErrorResponse(400, "Bad Request", e.what());
        }
        catch (const std::exception &e)
        {
            SPDLOG_LOGGER_ERROR(logger,
                                "Failed to parse POST body: {}",
                                e.what());
           return ::toErrorResponse(500,
                                    "Internal Server Error",
                                    "Internal error");
        }
        // Is every request in the 204 cache?
        if (noDataCache != nullptr)
        {
            bool allIn204Cache{true};
            for (const auto &request : *bulkRequest) 
            {
                try
                {
                    auto cacheKey = request.toString();
                    if (!noDataCache->contains(cacheKey))
                    {
                        allIn204Cache = false;
                        break;
                    }
                }
                catch (const std::exception &e)
                {
                    SPDLOG_LOGGER_WARN(logger,
                                        "Failed to check cache because {}",
                                        e.what());
                    // At this stage we don't know if this in the cache
                    // so give up and push forward
                    allIn204Cache = false;
                    break;
                }
            }
            if (allIn204Cache)
            {
                metrics.increment204CacheHitCounter();
                SPDLOG_LOGGER_DEBUG(logger, "204 cache hit for POST request");
                return crow::response{204};
            }
        }
        // Rate limit?
        if (rateLimiter.rateExceeded())
        {
            metrics.incrementRateLimitedRequests();
            SPDLOG_LOGGER_DEBUG(logger,
                                "Rate limited POST request");
            auto response = ::toErrorResponse(503,
                                              "Service Unavailable",
                                              "Rate limit exceeded");
            response.set_header("Retry-After", "1");
            return response;
        }
        // POST
        auto response
            = ::toCrowResponse(earthscopeClient.post(*bulkRequest), *logger);
        const auto &first = *bulkRequest->begin();
        if (response.code == 200)
        {
            auto elapsedTime = std::chrono::steady_clock::now() - startClock;
            Metrics::recordClientRequestDuration(
                "post",
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                   elapsedTime));
            metrics.incrementSuccessCounter("post");
            SPDLOG_LOGGER_INFO(
                logger,
                "POST {} selections (first {}.{}.{}.{}) -> {} ({} bytes, {} ms)",
                bulkRequest->size(),
                first.getNetwork().value_or("*"),
                first.getStation().value_or("*"),
                first.getLocationCode().value_or("*"),
                first.getChannel().value_or("*"),
                response.code,
                response.body.size(),
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    elapsedTime).count());
        }
        else if (response.code == 204)
        {
            if (noDataCache != nullptr && !bulkRequest->noDataIs404())
            {   
                try
                {
                    // Hopefully we don't blow out the cache
                    for (const auto &request : *bulkRequest)
                    {
                        noDataCache->insert(request.toString());
                    }
                }   
                catch (const std::exception &e) 
                {   
                    SPDLOG_LOGGER_WARN(logger,
                                       "Failed to update cache because {}",
                                       e.what() );
                }
            }
            int code = bulkRequest->noDataIs404() ? 404 : 204; 
            auto elapsedTime = std::chrono::steady_clock::now() - startClock;
            Metrics::recordClientRequestDuration(
                "post",
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                   elapsedTime));
            SPDLOG_LOGGER_INFO(
                logger,
                "POST {} selections generated no data (first {}.{}.{}.{}) -> {} ({} ms)",
                bulkRequest->size(),
                first.getNetwork().value_or("*"),
                first.getStation().value_or("*"),
                first.getLocationCode().value_or("*"),
                first.getChannel().value_or("*"),
                code,
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    elapsedTime).count());
             if (bulkRequest->noDataIs404())
             {
                 return ::toErrorResponse(404, "Not Found", "No data available");
             }
        }
        else
        {
            auto elapsedTime = std::chrono::steady_clock::now() - startClock;
            Metrics::recordClientRequestDuration(
                "post",
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                   elapsedTime));
            SPDLOG_LOGGER_WARN(
                logger,
                "POST {} selections (first {}.{}.{}.{}) -> {} ({} ms) failed",
                bulkRequest->size(),
                first.getNetwork().value_or("*"),
                first.getStation().value_or("*"),
                first.getLocationCode().value_or("*"),
                first.getChannel().value_or("*"),
                response.code,
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    elapsedTime).count());
        }
        return response;
    });

    try
    {
        SPDLOG_LOGGER_INFO(customLogger.logger,
                           "Starting Crow on {}:{}",
                           programOptions.crowOptions.bindAddress,
                           programOptions.crowOptions.port);
        if (programOptions.crowOptions.verbosity <= 1)
        {
            app.loglevel(crow::LogLevel::Critical);
        }
        else if (programOptions.crowOptions.verbosity == 2)
        {
            app.loglevel(crow::LogLevel::Warning);
        }
        else if (programOptions.crowOptions.verbosity == 3)
        {
            app.loglevel(crow::LogLevel::Info);
        }
        else
        {
            app.loglevel(crow::LogLevel::Debug);
        }
        app.bindaddr(programOptions.crowOptions.bindAddress)
           .port(programOptions.crowOptions.port)
           .server_name(programOptions.crowOptions.serverName);
        app.concurrency(programOptions.crowOptions.nThreads);
        if (programOptions.crowOptions.nThreads > 1)
        {
            SPDLOG_LOGGER_INFO(consoleLogger, "Enabling concurrency");
            app.multithreaded();
        }
/*
#ifdef CROW_ENABLE_COMPRESSION
        if (programOptions.crowOptions.useCompression)
        {
            SPDLOG_LOGGER_INFO(consoleLogger, "Enabling compression");
            app.use_compression(programOptions.crowOptions.compression);
        }
#endif
*/
#ifdef CROW_ENABLE_SSL
        if (programOptions.crowOptions.useSSL)
        {
            if (programOptions.crowOptions.useCertificateChain)
            {
                SPDLOG_LOGGER_INFO(consoleLogger, "Enabling SSL key chain");
                app.ssl_chainfile(
                   programOptions.crowOptions.certificateAndKeyFile.first,
                   programOptions.crowOptions.certificateAndKeyFile.second);
            }
            else
            {
                SPDLOG_LOGGER_INFO(consoleLogger, "Enabling SSL keys");
                app.ssl_file(
                   programOptions.crowOptions.certificateAndKeyFile.first,
                   programOptions.crowOptions.certificateAndKeyFile.second);
            }
        }
#endif
        app.run();
        if (!app.is_bound())
        {
            SPDLOG_LOGGER_CRITICAL(consoleLogger,
                                   "Crow failed to start on {}:{}",
                                   programOptions.crowOptions.bindAddress,
                                   programOptions.crowOptions.port);
            UEarthscopeDataselectAmbassador::Metrics::cleanup(); 
            UEarthscopeDataselectAmbassador::Logger::cleanup();
            return EXIT_FAILURE;
        }
        SPDLOG_LOGGER_INFO(consoleLogger, "Finished running Crow");
        //UEarthscopeDataselectAmbassador::Metrics::cleanup();
        UEarthscopeDataselectAmbassador::Logger::cleanup();
        return EXIT_SUCCESS;
    }
    catch (const std::exception &e)
    {
        SPDLOG_LOGGER_CRITICAL(consoleLogger, "Critical error: {}", e.what());
        UEarthscopeDataselectAmbassador::Metrics::cleanup();
        UEarthscopeDataselectAmbassador::Logger::cleanup();
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
