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
#include "uEarthscopeDataselectAmbassador/curlGlobal.hpp"
#include "uEarthscopeDataselectAmbassador/metricsSingleton.hpp"
#include "uEarthscopeDataselectAmbassador/version.hpp"
#include "grpcServer/parseCommandLineOptions.hpp"
#include "grpcServer/programOptions.hpp"


int main(int argc, char *argv[])
{
    using namespace UEarthscopeDataselectAmbassador;
    const CURLGlobal curlGlobal;
    Metrics::initializeMetricsSingleton();
    //NOLINTNEXTLINE(misc-include-cleaner)
    auto consoleLogger = spdlog::stdout_color_st("console");
    SPDLOG_LOGGER_INFO(consoleLogger,
                       "Running version {} of {}",
                       Version::getVersionWithTag(),
                       APPLICATION_NAME);

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


    return EXIT_SUCCESS;
}
