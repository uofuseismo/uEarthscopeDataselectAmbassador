#include <stdexcept>
#include <curl/curl.h>
#include "uEarthscopeDataselectAmbassador/curlGlobal.hpp"

using namespace UEarthscopeDataselectAmbassador;

/// Constructor
CURLGlobal::CURLGlobal()
{
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
    {
        throw std::runtime_error("curl_global_init failed");
    }
}
 
/// Destructor
CURLGlobal::~CURLGlobal()
{
    curl_global_cleanup();
}
