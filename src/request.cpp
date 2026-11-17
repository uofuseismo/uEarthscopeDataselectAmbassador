#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <format>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <boost/url/encoding_opts.hpp>
#include <boost/url/params_view.hpp>
#include <boost/url/parse_query.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include "uEarthscopeDataselectAmbassador/request.hpp"

using namespace UEarthscopeDataselectAmbassador;

namespace
{

[[nodiscard]] std::string toISO8601(const std::chrono::microseconds &microseconds)
{
    const std::chrono::sys_time<std::chrono::microseconds> time{microseconds};
    return std::format("{:%FT%T}", time);
}

[[nodiscard]] std::string toString(const Request::DataFormat format)
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

[[nodiscard]] Request::DataFormat toDataFormat(const std::string &format)
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

enum class CodeType : std::uint8_t 
{
    Network,
    Station,
    Location,
    Channel
};

[[nodiscard]] constexpr size_t toMaxLength(const CodeType type) noexcept
{
    switch (type)
    {
        case CodeType::Network:  return 2;  // SEED 2.4 limits
        case CodeType::Station:  return 5;
        case CodeType::Location: return 2;
        case CodeType::Channel:  return 3;
    }
    return 0;
}

[[nodiscard]] constexpr std::string_view toName(const CodeType type) noexcept
{
    switch (type)
    {
        case CodeType::Network:  return "Network";
        case CodeType::Station:  return "Station";
        case CodeType::Location: return "Location";
        case CodeType::Channel:  return "Channel";
    }
    return "Unknown";
}

/// Alphanumerics plus the FDSN wildcards.  Assumes upper case.
[[nodiscard]] constexpr bool isAllowed(const char c) noexcept
{
    return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
        || c == '?' || c == '*';
}

/// Validates and normalizes a single code (no commas).
[[nodiscard]] std::string normalizeItem(const std::string_view item,
                                        const CodeType type)
{
    std::string result;
    result.reserve(item.size());
    for (const auto c : item)
    {
        result.push_back(
            static_cast<char> (std::toupper(static_cast<unsigned char> (c))));
    }
    // Blank location codes are spelled "--" in FDSN queries
    if (type == CodeType::Location &&
        std::ranges::all_of(result, [](const char c){return c == ' ';}))
    {
        return "--";
    }
    if (type == CodeType::Location && result == "--"){return result;}
    if (result.empty())
    {
        throw std::invalid_argument(
            std::format("{} code is empty", toName(type)));
    }
    if (result.size() > toMaxLength(type))
    {
        throw std::invalid_argument(
            std::format("{} code exceeds {} characters",
                        toName(type), toMaxLength(type)));
    }
    if (!std::ranges::all_of(result, isAllowed))
    {
        // Deliberately don't echo the input; it may end up in a 400 body
        throw std::invalid_argument(
            std::format("{} code may only contain A-Z, 0-9, ?, and *",
                        toName(type)));
    }
    return result;
}

/*
/// Validates a code or comma-separated list of codes and returns the
/// normalized (upper-case) form.
/// @throws std::invalid_argument if any item is invalid.
[[nodiscard]] std::string normalizeCode(const std::string_view code,
                                        const CodeType type)
{
    constexpr size_t maxItems{16};
    std::string result;
    size_t nItems{0};
    size_t start{0};
    while (true)
    {
        const auto comma = code.find(',', start);
        const auto item = code.substr(start,
            comma == std::string_view::npos ? std::string_view::npos
                                            : comma - start);
        if (++nItems > maxItems)
        {
            throw std::invalid_argument(
                std::format("{} list exceeds {} items",
                            toName(type), maxItems));
        }
        if (nItems > 1){result.push_back(',');}
        result.append(normalizeItem(item, type));
        if (comma == std::string_view::npos){break;}
        start = comma + 1;
    }
    return result;
}
*/

enum class Parameter : std::uint8_t
{ 
    Network = 1,
    Station = 2,
    Location = 3,
    Channel = 4,
    Start = 5,
    End = 6,
    Format = 7,
    NoData = 8
};

[[nodiscard]] std::string toString(const Parameter parameter)
{
    if (parameter == Parameter::Network)
    {
        return "net[work]";
    }
    else if (parameter == Parameter::Station)
    {
        return "sta[tion]";
    }
    else if (parameter == Parameter::Location)
    {
        return "loc[ation]";
    }
    else if (parameter == Parameter::Channel)
    {
        return "cha[nnel]";
    }
    else if (parameter == Parameter::Start) 
    {
        return "start[time]";
    }
    else if (parameter == Parameter::End)
    {
        return "end[time]";
    }
    else if (parameter == Parameter::Format)
    {
        return "format";
    }
    else if (parameter == Parameter::NoData)
    {
        return "nodata";
    }
    return "undefined";
}


[[nodiscard]] std::optional<Parameter> toParameter(std::string_view key)
{
    constexpr std::array<std::pair<std::string_view, Parameter>, 14> names
    {{
        {"net", Parameter::Network},   {"network", Parameter::Network},
        {"sta", Parameter::Station},   {"station", Parameter::Station},
        {"loc", Parameter::Location},  {"location", Parameter::Location},
        {"cha", Parameter::Channel},   {"channel", Parameter::Channel},
        {"start", Parameter::Start},   {"starttime", Parameter::Start},
        {"end", Parameter::End},       {"endtime", Parameter::End},
        {"format", Parameter::Format},
        {"nodata", Parameter::NoData}
    }};
    for (const auto &[name, parameter] : names)
    {
        if (boost::algorithm::iequals(key, name)){return parameter;}
    }
    return std::nullopt;
}

[[nodiscard]] bool isRetired(std::string_view key)
{
    return boost::algorithm::iequals(key, "quality")
        || boost::algorithm::iequals(key, "minimumlength")
        || boost::algorithm::iequals(key, "longestonly");
}

[[nodiscard]] int toDigits(const std::string_view time,
                           const size_t position, const size_t width)
{
    if (position + width > time.size())
    {
        throw std::invalid_argument("Time is truncated");
    }
    int value{0};
    for (const auto c : time.substr(position, width))
    {
        if (c < '0' || c > '9')
        {
            throw std::invalid_argument(
               "Time has a non-digit where a digit belongs");
        }
        value = 10*value + (c - '0');
    }
    return value;
}


[[nodiscard]] std::chrono::microseconds parseTime(std::string_view time)
{
    if (time.ends_with('Z')){time.remove_suffix(1);}
    if (time.size() < 10 || time[4] != '-' || time[7] != '-')
    {
        throw std::invalid_argument("Time must be YYYY-MM-DD[THH:MM:SS[.ffffff]]");
    }
    const std::chrono::year_month_day date{
        std::chrono::year {::toDigits(time, 0, 4)},
        std::chrono::month {static_cast<unsigned int> (::toDigits(time, 5, 2))},
        std::chrono::day {static_cast<unsigned int> (::toDigits(time, 8, 2))}};
    if (!date.ok()){throw std::invalid_argument("Time has an invalid date");}
    std::chrono::microseconds result{
        std::chrono::sys_days {date}.time_since_epoch()};
    if (time.size() == 10){return result;}

    if ((time[10] != 'T' && time[10] != ' ') || time.size() < 19 ||
        time[13] != ':' || time[16] != ':')
    {
        throw std::invalid_argument(
           "Time must be YYYY-MM-DD[THH:MM:SS[.ffffff]]");
    }
    const auto hour = ::toDigits(time, 11, 2);
    const auto minute = ::toDigits(time, 14, 2);
    const auto second = ::toDigits(time, 17, 2);
    if (hour > 23 || minute > 59 || second > 59)
    {
        throw std::invalid_argument("Time of day is out of range");
    }
    result += std::chrono::hours {hour} + std::chrono::minutes {minute}
            + std::chrono::seconds {second};
    if (time.size() == 19){return result;}

    const auto width = time.size() - 20;
    if (time[19] != '.' || width < 1 || width > 6)
    {
        throw std::invalid_argument("Fractional seconds must be 1 to 6 digits");
    }
    auto microseconds = ::toDigits(time, 20, width);
    for (auto i = width; i < 6; ++i){microseconds *= 10;}
    return result + std::chrono::microseconds {microseconds};
}

}

class Request::RequestImpl
{
public:
    std::optional<std::string> mNetwork{std::nullopt};
    std::optional<std::string> mStation{std::nullopt};
    std::optional<std::string> mChannel{std::nullopt};
    std::optional<std::string> mLocationCode{std::nullopt};
    std::pair<std::chrono::microseconds,
              std::chrono::microseconds> mStartAndEndTime;
    DataFormat mDataFormat{DataFormat::MiniSEED};
    bool mHasStartAndEndTime{false};
    bool mNoDataIs404{false};
};

/// Constructor
Request::Request() :
    pImpl(std::make_unique<RequestImpl> ())
{
}

/// Construct from a query string
Request::Request(const std::string &query)
{
    Request request;
    const auto encoded = boost::urls::parse_query(query);
    if (!encoded){throw std::invalid_argument("Malformed query string");}
    boost::urls::encoding_opts options;
    options.space_as_plus = true;
    const boost::urls::params_view parameters{*encoded, options};

    std::optional<std::chrono::microseconds> startTime;
    std::optional<std::chrono::microseconds> endTime;
    std::set<std::uint8_t> seenKey;
    for (const auto &parameter : parameters)
    {
        const auto whichParameter = ::toParameter(parameter.key);
        if (whichParameter == std::nullopt)
        {
            if (::isRetired(parameter.key)){continue;}
            throw std::invalid_argument("Unsupported parameter");
        }
        else
        {
            if (seenKey.contains(static_cast<std::uint8_t> (*whichParameter)))
            {
                throw std::invalid_argument(
                   ::toString(*whichParameter) +" duplicated");
            }
            seenKey.insert(static_cast<std::uint8_t> (*whichParameter));
        }
        if (!parameter.has_value || parameter.value.empty())
        {
            if (*whichParameter != Parameter::Location)
            {
                throw std::invalid_argument("Parameter "
                                          + ::toString(*whichParameter)
                                          + " requires a value");
            }
        }
        if (*whichParameter == ::Parameter::Network)
        {
            request.setNetwork(parameter.value);
        }
        else if (*whichParameter == ::Parameter::Station)
        {
            request.setStation(parameter.value);
        }
        else if (*whichParameter  == ::Parameter::Channel)
        {
            request.setChannel(parameter.value);
        }   
        else if (*whichParameter == Parameter::Location)
        {
            request.setLocationCode(parameter.value);
        }
        else if (*whichParameter == Parameter::Start)
        {
            startTime = ::parseTime(parameter.value);
        }   
        else if (*whichParameter == Parameter::End)
        {
            endTime = ::parseTime(parameter.value);
        }   
        else if (*whichParameter == Parameter::Format)
        {
            request.setDataFormat(::toDataFormat(parameter.value));
        }
        else if (*whichParameter == Parameter::NoData)
        {
            if (parameter.value == "404")
            {
                request.enableNoDataIs404();
            }
            else if (parameter.value == "204")
            {
                request.disableNoDataIs404();
            }
            else
            {
                throw std::invalid_argument("nodata must be 204 or 404");
            }
        }
        else
        {
            throw std::runtime_error("Could not classify parameter");
        }
    }
    if (!startTime || !endTime)
    {
        throw std::invalid_argument("start and end are required");
    }
    request.setStartAndEndTime(std::pair {*startTime, *endTime});
    *this = std::move(request);
}

/// Copy constructor
Request::Request(const Request &request)
{
    *this = request;
}

/// Move constructor
Request::Request(Request &&request) noexcept
{
    *this = std::move(request);
}

/// Copy assignment
Request& Request::operator=(const Request &request)
{
    if (&request == this){return *this;}
    pImpl = std::make_unique<RequestImpl> (*request.pImpl);
    return *this;
}

/// Move assignment
Request& Request::operator=(Request &&request) noexcept
{
    if (&request == this){return *this;}
    pImpl = std::move(request.pImpl);
    return *this;
}

/// Destructor
Request::~Request() = default;

/// Start and end time
void Request::setStartAndEndTime(
    const std::pair<std::chrono::microseconds,
                    std::chrono::microseconds> &startAndEndTime)
{
    if (startAndEndTime.second <= startAndEndTime.first)
    {
        throw std::invalid_argument("Start time must be precede end time");
    }
    pImpl->mStartAndEndTime = startAndEndTime;
    pImpl->mHasStartAndEndTime = true;
}

std::pair<std::chrono::microseconds, std::chrono::microseconds>
Request::getStartAndEndTime() const
{
    if (!hasStartAndEndTime())
    {
        throw std::runtime_error("Start and end time not set");
    }
    return pImpl->mStartAndEndTime;
}


bool Request::hasStartAndEndTime() const noexcept
{
    return pImpl->mHasStartAndEndTime;
}


/// Network
void Request::setNetwork(const std::string &networkIn)
{
    if (!networkIn.empty())
    {
        auto network = ::normalizeItem(networkIn, ::CodeType::Network);
        pImpl->mNetwork = std::make_optional<std::string> (network);
    }
}

std::optional<std::string> Request::getNetwork() const noexcept
{
    return pImpl->mNetwork;
}

/// Station
void Request::setStation(const std::string &stationIn)
{
    if (!stationIn.empty())
    {
        auto station = ::normalizeItem(stationIn, ::CodeType::Station);
        pImpl->mStation = std::make_optional<std::string> (station);
    }
}

std::optional<std::string> Request::getStation() const noexcept
{
    return pImpl->mStation;
}

/// Channel
void Request::setChannel(const std::string &channelIn)
{
    if (!channelIn.empty())
    {
        auto channel = ::normalizeItem(channelIn, ::CodeType::Channel);
        pImpl->mChannel = std::make_optional<std::string> (channel);
    }   
}

std::optional<std::string> Request::getChannel() const noexcept
{
    return pImpl->mChannel;
}

/// Location code
void Request::setLocationCode(const std::string &locationCodeIn)
{
    if (!locationCodeIn.empty())
    {
        auto locationCode
            = ::normalizeItem(locationCodeIn, ::CodeType::Location);
        pImpl->mLocationCode = std::make_optional<std::string> (locationCode);
    }
    else
    {
        pImpl->mLocationCode = "--";
    }
}

std::optional<std::string> Request::getLocationCode() const noexcept
{
    return pImpl->mLocationCode;
}


/// Data format
void Request::setDataFormat(const DataFormat format) noexcept
{
    pImpl->mDataFormat = format;
}

Request::DataFormat Request::getDataFormat() const noexcept
{
    return pImpl->mDataFormat;
}

/// Remap 204 to 404?
void Request::enableNoDataIs404() noexcept
{
    pImpl->mNoDataIs404 = true;
}

void Request::disableNoDataIs404() noexcept
{
    pImpl->mNoDataIs404 = false;
}

bool Request::noDataIs404() const noexcept
{
    return pImpl->mNoDataIs404;
}


/// To string
std::string Request::toString() const
{
    if (!hasStartAndEndTime())
    {
        throw std::runtime_error("Start and end time not set");
    }

    // URL says it's required - start here so I don't need annoying
    // logic to append amperstands
    std::string query;
    auto [startTime, endTime] = getStartAndEndTime();
    query.append("start=" + ::toISO8601(startTime));
    query.append("&end=" + ::toISO8601(endTime));

    auto network = getNetwork();
    if (network != std::nullopt)
    {
        query.append("&net=" + *network); 
    }
    auto station = getStation();
    if (station != std::nullopt)
    {
        query.append("&sta=" + *station);
    }
    auto channel = getChannel();
    if (channel != std::nullopt)
    {
        query.append("&cha=" + *channel);
    }
    auto locationCode = getLocationCode();
    if (locationCode != std::nullopt)
    {
        query.append("&loc=" + *locationCode);
    }

    query.append("&" + ::toString(getDataFormat()));

    if (noDataIs404())
    {
        query.append("&nodata=404");
    }
    else
    {
        query.append("&nodata=204");
    }
    return query;
}
