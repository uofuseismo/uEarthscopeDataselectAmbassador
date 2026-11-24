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
#include "utilities/otelOptions.hpp"

namespace
{
constexpr std::string_view APPLICATION_NAME{"uEarthscopeDataselectGRPCAmbassador"};
constexpr int MAX_REQUESTS_PER_SECOND{50};

}
#endif
