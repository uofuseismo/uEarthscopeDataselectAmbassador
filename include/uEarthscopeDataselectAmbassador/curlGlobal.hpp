#ifndef UEARTHSCOPE_DATASELECT_AMBASSADOR_CURL_GLOBAL_HPP
#define UEARTHSCOPE_DATASELECT_AMBASSADOR_CURL_GLOBAL_HPP
namespace UEarthscopeDataselectAmbassador
{
/// @class CURLGlobal curlGlobal.hpp
/// @brief Handles cURL global init and cleanup operations once and for all
///        for the application.  Instantiate this class at the beginning
///        of your main application.
/// @copyright Ben Baker (University of Utah) distributed under the
///            MIT NO AI license.
class CURLGlobal
{
public:
    CURLGlobal();
    ~CURLGlobal();
    CURLGlobal(const CURLGlobal &) = delete;
    CURLGlobal(CURLGlobal &&) = delete;
    CURLGlobal& operator=(const CURLGlobal &) = delete;
    CURLGlobal& operator=(CURLGlobal &&) = delete;
};
}
#endif
