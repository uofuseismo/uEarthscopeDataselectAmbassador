#ifndef CROW_HELPER_HPP
#define CROW_HELPER_HPP
#include <chrono>
#include <expected>
#include <format>
#include <string>
#include <spdlog/spdlog.h>
#include <spdlog/logger.h>
#include <crow/http_response.h>
#include "uEarthscopeDataselectAmbassador/earthscopeClient.hpp"
#include "uEarthscopeDataselectAmbassador/metricsSingleton.hpp"
#include "uEarthscopeDataselectAmbassador/request.hpp"

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
        UEarthscopeDataselectAmbassador::Metrics
            ::MetricsSingleton::getInstance().incrementClientUnauthorizedCounter();
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

#endif
