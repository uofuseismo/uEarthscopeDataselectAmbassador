#include <cstdlib>
#include <filesystem>
#include <exception>
#include <memory>
#include <stdexcept>
#include <utility>
#include <stdlib.h> // setenv
#include <spdlog/spdlog.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h> //NOLINT
#include <crow/app.h>
#include <crow/http_request.h>
#include <crow/http_response.h>
#include <crow/logging.h>
#include "uEarthscopeDataselectAmbassador/earthscopeClient.hpp"
//NOLINTNEXTLINE(misc-include-cleaner)
#include "uEarthscopeDataselectAmbassador/earthscopeClientOptions.hpp"
#include "uEarthscopeDataselectAmbassador/metricsSingleton.hpp"
#include "uEarthscopeDataselectAmbassador/version.hpp"
#include "parseCommandLineOptions.hpp"
#include "programOptions.hpp"
#include "logger.hpp"

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
        else if (level == crow::LogLevel::Critical)
        {
            logger->critical(message);
        }
        else if (level == crow::LogLevel::Error)
        {
            logger->error(message);
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

    // Parse that ini file
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

    // Create the star of the show
    EarthscopeClient
        earthscopeClient{programOptions.earthscopeClientOptions, logger};

    return EXIT_SUCCESS;
}
