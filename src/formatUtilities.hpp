#ifndef UEARTHSCOPE_DATASELECT_AMBASSADOR_FORMAT_UTILITIES_HPP
#define UEARTHSCOPE_DATASELECT_AMBASSADOR_FORMAT_UTILITIES_HPP
#include <chrono>
#include <format>
#include <stdexcept>
#include <string>
#include "uEarthscopeDataselectAmbassador/request.hpp"

namespace UEarthscopeDataselectAmbassador::Details
{
[[nodiscard]] inline std::string toISO8601(const std::chrono::microseconds &microseconds)
{
    const std::chrono::sys_time<std::chrono::microseconds> time{microseconds};
    return std::format("{:%FT%T}", time);
}

[[nodiscard]] inline std::string toString(
    const UEarthscopeDataselectAmbassador::Request::DataFormat format)
{
    if (format == Request::DataFormat::MiniSEED)
    {   
        return "format=miniseed";
    }   
    else if (format == Request::DataFormat::GeoCSV)
    {   
        return "format=geocsv";
    }   
    else if (format == Request::DataFormat::GeoCSV_INLINE)
    {   
        return "format=geocsv.inline";
    }   
    else if (format == Request::DataFormat::GeoCSV_ZIP)
    {   
        return "format=geocsv.zip";
    }   
    else if (format == Request::DataFormat::GeoCSV_TSPAIR)
    {   
        return "format=geocsv.tspair";
    }   
    else if (format == Request::DataFormat::GeoCSV_SLIST)
    {   
        return "format=geocsv.slist";
    }   
    throw std::runtime_error("Unhandled data format");
}


[[nodiscard]] inline Request::DataFormat toDataFormat(const std::string &format)
{
    if (format == "miniseed")
    {
        return Request::DataFormat::MiniSEED; 
    }
    else if (format == "geocsv")
    {   
        return Request::DataFormat::GeoCSV;
    }
    else if (format == "geocsv.inline")
    {   
        return Request::DataFormat::GeoCSV_INLINE;
    }   
    else if (format == "geocsv.zip")
    {
        return Request::DataFormat::GeoCSV_ZIP;
    }
    else if (format == "geocsv.tspair")
    {   
        return Request::DataFormat::GeoCSV_TSPAIR;
    }
    else if (format == "geocsv.slist")
    {   
        return Request::DataFormat::GeoCSV_SLIST;
    }   
    throw std::invalid_argument(
        "format must be miniseed, geocsv, geocsv.inline, geocsv.zip, geocsv.tspair, or geocsv.slist");
}

}
#endif
