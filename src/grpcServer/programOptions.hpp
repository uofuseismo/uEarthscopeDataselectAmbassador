#ifndef PROGRAM_OPTIONS_HPP
#define PROGRAM_OPTIONS_HPP
#include <filesystem>
#include <string>
#include <string_view>
#include <boost/system/error_code.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ptree_fwd.hpp>
#include <boost/property_tree/ini_parser.hpp>
#ifdef CROW_ENABLE_COMPRESSION
#include <crow/compression.h>
#endif
#include "uEarthscopeDataselectAmbassador/earthscopeClientOptions.hpp"
#include "uEarthscopeDataselectAmbassador/noDataCacheOptions.hpp"
#include "uEarthscopeDataselectAmbassador/grpc/serverOptions.hpp"
#include "utilities/otelOptions.hpp"

namespace
{
constexpr std::string_view APPLICATION_NAME{"uEarthscopeDataselectGRPCAmbassador"};
constexpr int MAX_REQUESTS_PER_SECOND{50};

struct ProgramOptions
{
    void parseInitializationFile(const std::filesystem::path &iniFile)
    {   
        using namespace UEarthscopeDataselectAmbassador;
        if (!std::filesystem::exists(iniFile))
        {
            throw std::invalid_argument("Initialization file "
                                      + std::string {iniFile}
                                      + " does not exist");
        }

        // Parse the initialization file
        boost::property_tree::ptree propertyTree;
        boost::property_tree::ini_parser::read_ini(iniFile, propertyTree);
        applicationName
            = propertyTree.get<std::string> ("General.application",
                                             applicationName);
        if (applicationName.empty()){applicationName = APPLICATION_NAME;}
        verbosity
            = propertyTree.get<int> ("General.verbosity", verbosity);

        // Earthscope creds
        earthscopeClientOptions
            = EarthscopeClientOptions::fromInitializationFile(
                 iniFile, "Earthscope"); 

        // No data cache
        noDataCacheOptions
            = NoDataCacheOptions::fromInitializationFile(
                 iniFile, "NoDataCache"); 

        // Rate limit
        maximumRequestsPerSecond
            = propertyTree.get<int> ("RateLimiter.maximumRequestsPerSecond",
                                     MAX_REQUESTS_PER_SECOND);
        if (maximumRequestsPerSecond < 1)
        {
            throw std::invalid_argument(
                "RateLimiter.maximumRequestsPerSecond must be positive");
        }

        // gRPC server
        grpcServerOptions
            = GRPC::ServerOptions::fromInitializationFile(
                iniFile, "GRPCServer");

        // Get OTel logs options
        auto httpLog
            = OTelOptions::getHTTPLogOptionsFromIniFile(
                propertyTree, "OTelHTTPLogOptions");
        exportLogs = false;
        if (httpLog != std::nullopt)
        {
            otelHTTPLogOptions = *httpLog;
            exportLogs = true;
            exportLogsWithHTTP = true;
        }
        else
        {
            auto grpcLog
                = OTelOptions::getGRPCLogOptionsFromIniFile(
                    propertyTree, "OTelGRPCLogOptions");
            if (grpcLog != std::nullopt)
            {
                otelGRPCLogOptions = *grpcLog;
                exportLogs = true;
                exportLogsWithHTTP = false;
            }
        }

        // Get OTel metrics options
        auto httpMetrics
            = OTelOptions::getHTTPMetricsOptionsFromIniFile(
                propertyTree, "OTelHTTPMetricsOptions");
        exportMetrics = false;
        if (httpMetrics != std::nullopt)
        {
            otelHTTPMetricsOptions = *httpMetrics;
            exportMetrics = true;
            exportMetricsWithHTTP = true;
        }
        else
        {
            auto grpcMetrics
                = OTelOptions::getGRPCMetricsOptionsFromIniFile(
                    propertyTree, "OTelGRPCMetricsOptions");
            if (grpcMetrics != std::nullopt)
            {
                otelGRPCMetricsOptions = *grpcMetrics;
                exportMetrics = true;
                exportMetricsWithHTTP = false;
            }
        }
    }

    std::string applicationName{APPLICATION_NAME};
    UEarthscopeDataselectAmbassador::OTelOptions::HTTPMetrics
        otelHTTPMetricsOptions;
    UEarthscopeDataselectAmbassador::OTelOptions::HTTPLog
        otelHTTPLogOptions;
    UEarthscopeDataselectAmbassador::OTelOptions::GRPCMetrics
        otelGRPCMetricsOptions;
    UEarthscopeDataselectAmbassador::OTelOptions::GRPCLog
        otelGRPCLogOptions;
    UEarthscopeDataselectAmbassador::EarthscopeClientOptions
        earthscopeClientOptions;
    UEarthscopeDataselectAmbassador::NoDataCacheOptions
        noDataCacheOptions;
    UEarthscopeDataselectAmbassador::GRPC::ServerOptions
        grpcServerOptions;
    std::string earthscopeUsername;
    std::string earthscopePassword;
    std::string earthscopeToken;
    int maximumRequestsPerSecond{MAX_REQUESTS_PER_SECOND};
    int verbosity{3};
    bool documentAPI{false};
    bool exportLogs{false};
    bool exportLogsWithHTTP{true};
    bool exportMetrics{false};
    bool exportMetricsWithHTTP{true};
};

}
#endif
