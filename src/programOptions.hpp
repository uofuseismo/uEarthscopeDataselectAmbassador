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
#include "otelOptions.hpp"

namespace
{
constexpr std::string_view APPLICATION_NAME{"uEarthscopeDataselectAmbassador"};

struct CrowOptions
{
    static CrowOptions 
        parseInitializationFile(boost::property_tree::ptree &propertyTree,
                                const std::string &sectionIn = "Crow")
    {
        auto section = sectionIn;
        if (section.empty()){section = "Crow";}
        if (section.back() != '.'){section.append(".");}
        CrowOptions options;
        options.bindAddress
            = propertyTree.get<std::string> (section + "address",
                                             options.bindAddress);
        if (options.bindAddress.empty())
        {
            throw std::invalid_argument("Crow bind address is empty");
        }

        options.port
            = propertyTree.get<uint16_t> (section + "port",
                                          options.port);

        options.serverName
            = propertyTree.get<std::string> (section + "serverName",
                                             options.serverName);
        if (options.serverName.empty())
        {
            throw std::invalid_argument("Crow server name is empty");
        }

        options.nThreads
            = propertyTree.get<uint16_t>
                  (section + "numberOfThreads",
                   static_cast<uint16_t> (options.nThreads));
        if (options.nThreads < 1)
        {
            throw std::invalid_argument("Crow number of threads not positive");
        }

/*
#ifdef CROW_ENABLE_COMPRESSION
        auto compressionType
           = propertyTree.get<std::string> (section + "compression",
                                            "none");
        std::transform(compressionType.begin(),
                       compressionType.end(),
                       compressionType.begin(),
                       ::tolower);
        if (compressionType == "none")
        {
            options.useCompression = false;
        }
        else if (compressionType == "zlib")
        {
            options.useCompression = true;
            options.compression = crow::compression::algorithm::DEFLATE;
        }
        else if (compressionType == "gzip")
        {
            options.useCompression = true;
            options.compression = crow::compression::algorithm::GZIP;
        }
        else
        {
            throw std::invalid_argument("Unhandled compression type: "
                                       + compressionType);
        }
#endif
*/
#ifdef CROW_ENABLE_SSL
        auto certificateFile
            = propertyTree.get_optional<std::string>
              (section + "certificateFile");
        auto certificateKey
            = propertyTree.get_optional<std::string>
              (section + "certificateKey");
        if (certificateFile && certificateKey)
        {
            if (!std::filesystem::exists(*certificateFile))
            {
                throw std::invalid_argument("Certificate "
                                          + *certificateFile
                                          + " does not exist");
            }
            if (!std::filesystem::exists(*certificateKey))
            {
                throw std::invalid_argument("Certificate "
                                          + *certificateKey
                                          + " does not exist");
            }
            options.certificateAndKeyFile
                = std::make_pair(*certificateFile, *certificateKey);
            options.useCertificateChain = false;
            options.useSSL = true;
        }
        else
        {
            auto certificateChainFile
               = propertyTree.get_optional<std::string>
                 (section + "certificateChainFile");
            auto certificateChainKey
               = propertyTree.get_optional<std::string>
                 (section + "certificateChainKey");
            if (certificateChainFile && certificateChainKey)
            {
                if (!std::filesystem::exists(*certificateChainFile))
                {
                    throw std::invalid_argument("Certificate chain "
                                              + *certificateChainFile
                                              + " does not exist");
                }
                if (!std::filesystem::exists(*certificateChainKey))
                {
                    throw std::invalid_argument("Certificate chain "
                                              + *certificateChainKey
                                              + " does not exist");
                }
                options.certificateAndKeyFile
                    = std::make_pair(*certificateChainFile,
                                     *certificateChainKey);
                options.useCertificateChain = true;
                options.useSSL = true;
            }
        }
#endif
        return options;
    }

    std::string bindAddress{"127.0.0.1"};
    std::string serverName{"uearthscope-dataselect-ambassador-server"};
    int nThreads{1};
    uint16_t port{8080};
#ifdef CROW_ENABLE_COMPRESSION
    crow::compression::algorithm
        compression{crow::compression::algorithm::GZIP};
    bool useCompression{true};
#else
    bool useCompression{false};
#endif
#ifdef CROW_ENABLE_SSL
    std::pair<std::filesystem::path, std::filesystem::path> certificateAndKeyFile;
    bool useCertificateChain{false};
    bool useSSL{false};
#else
    bool useSSL{false};
#endif
};

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
            = propertyTree.get<std::string> ("General.applicationName",
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
                 iniFile, "NoDataCach"); 

        // Crow
        crowOptions
            = CrowOptions::parseInitializationFile(propertyTree, "Crow");

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
    CrowOptions crowOptions;
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
    std::string earthscopeUsername;
    std::string earthscopePassword;
    std::string earthscopeToken;
    int verbosity{3};
    bool documentAPI{false};
    bool exportLogs{false};
    bool exportLogsWithHTTP{true};
    bool exportMetrics{false};
    bool exportMetricsWithHTTP{true};
};

}
#endif
