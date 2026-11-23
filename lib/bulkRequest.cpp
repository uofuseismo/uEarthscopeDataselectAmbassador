#include <array>
#include <format>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/trim.hpp>
#include "uEarthscopeDataselectAmbassador/bulkRequest.hpp"
#include "uEarthscopeDataselectAmbassador/request.hpp"
#include "formatUtilities.hpp"

using namespace UEarthscopeDataselectAmbassador;

namespace
{
constexpr int MAXIMUM_SELECTIONS{1000};

[[nodiscard]] bool isRetired(std::string_view key)
{
    return boost::algorithm::iequals(key, "quality")
        || boost::algorithm::iequals(key, "minimumlength")
        || boost::algorithm::iequals(key, "longestonly");
}

}

class BulkRequest::BulkRequestImpl
{
public:
    std::vector<Request> mRequests;
    Request::DataFormat mDataFormat{Request::DataFormat::MiniSEED};
    bool mNoDataIs404{false};
};

/// Constructor
BulkRequest::BulkRequest() :
    pImpl(std::make_unique<BulkRequestImpl> ())
{
}

/// Copy constructor
BulkRequest::BulkRequest(const BulkRequest &request)
{
    *this = request;
}

/// Move constructor
BulkRequest::BulkRequest(BulkRequest &&request) noexcept
{
    *this = std::move(request);
}

/// Construct from string
BulkRequest::BulkRequest(const std::string &body)
{
    std::vector<Request> requests;
    std::istringstream stream{body};
    std::string line;
    std::string options;
    while (std::getline(stream, line))
    {
        boost::algorithm::trim(line);
        if (line.empty()){continue;}
        if (line.find_first_of("&%") != std::string::npos)
        {
            throw std::invalid_argument("Invalid character in line");
        }
        if (const auto equals = line.find('='); equals != std::string::npos)
        {
            auto key = boost::algorithm::trim_copy(line.substr(0, equals));
            auto value = boost::algorithm::trim_copy(line.substr(equals + 1));
            if (::isRetired(key)){continue;}
            if (!boost::iequals(key, "format") && !boost::iequals(key, "nodata"))
            {
                throw std::invalid_argument("Unsupported POST option");
            }
            //options.append("&" + line);
            //options.append("&" + key + "=" + value);
            options.append("&").append(key).append("=").append(value);
            continue;
        }
        std::istringstream fields{line};
        std::array<std::string, 6> field;
        std::string extra;
        if (!(fields >> field[0] >> field[1] >> field[2] >> field[3] >> field[4] >> field[5])
            || (fields >> extra))
        {
            throw std::invalid_argument("Selection lines need six fields");
        }
        if (requests.size() >= MAXIMUM_SELECTIONS)
        {
            throw std::invalid_argument("Too many selections");
        }
        requests.emplace_back("net=" + field[0] + "&sta=" + field[1]
                            + "&loc=" + field[2] + "&cha=" + field[3]
                            + "&start=" + field[4] + "&end=" + field[5]
                            + options);
    }
    if (requests.empty()){throw std::invalid_argument("No selections");}
    BulkRequest bulkRequest;
    bulkRequest.setRequests(std::move(requests));
    *this = std::move(bulkRequest);
}

/// Copy assignment
BulkRequest& BulkRequest::operator=(const BulkRequest &request)
{
    if (&request == this){return *this;}
    pImpl = std::make_unique<BulkRequestImpl> (*request.pImpl);
    return *this;
}

/// Move assignment
BulkRequest& BulkRequest::operator=(BulkRequest &&request) noexcept
{
    if (&request == this){return *this;}
    pImpl = std::move(request.pImpl);
    return *this;
}


/// Destructor
BulkRequest::~BulkRequest() = default;

/// Sets requests
void BulkRequest::setRequests(const std::vector<Request> &requests)
{
    auto copy = requests;
    setRequests(std::move(copy));
}

void BulkRequest::setRequests(std::vector<Request> &&requests)
{
    if (requests.empty())
    {
        throw std::invalid_argument("No requests set");
    }
    if (requests.size() > MAXIMUM_SELECTIONS)
    {
        throw std::invalid_argument("At most " 
                                  + std::to_string(MAXIMUM_SELECTIONS)
                                  + " allowed");
    }
    // Check duplicates
    std::set<std::string> stringRequests;
    for (const auto &request : requests)
    {
        if (!request.hasStartAndEndTime())
        {
            throw std::invalid_argument("Request start/end time not set");
        }
        auto stringRequest = request.toString();
        if (stringRequests.contains(stringRequest))
        {
            throw std::invalid_argument("Duplicate request detected");
        } 
        stringRequests.insert(std::move(stringRequest));
    }
    // Check for inconsistent return codes and data formats
    auto dataFormat = requests.at(0).getDataFormat();
    auto noDataIs404 = requests[0].noDataIs404();
    for (int i = 1; i < static_cast<int> (requests.size()); ++i)
    {
        if (dataFormat != requests[i].getDataFormat())
        {
            throw std::invalid_argument("Inconsistent data format");
        }
        if (noDataIs404 != requests[i].noDataIs404())
        {
            throw std::invalid_argument("Inconsistent 204/404");
        }
    }
    pImpl->mRequests = std::move(requests);
    pImpl->mDataFormat = dataFormat;
    pImpl->mNoDataIs404 = noDataIs404;
}

Request::DataFormat BulkRequest::getDataFormat() const noexcept
{
    return pImpl->mDataFormat;
}

bool BulkRequest::noDataIs404() const noexcept
{
    return pImpl->mNoDataIs404;
}

bool BulkRequest::empty() const noexcept
{
    return pImpl->mRequests.empty();
}

int BulkRequest::size() const noexcept
{
    return static_cast<int> (pImpl->mRequests.size());
}

/// To string
std::string BulkRequest::toString() const
{
    if (pImpl->mRequests.empty())
    {
        throw std::runtime_error("No requests set");
    }
    std::string result{Details::toString(getDataFormat())};
    result.append(noDataIs404() ? "\nnodata=404\n" : "\nnodata=204\n");
    for (const auto &request : pImpl->mRequests)
    {
        const auto [startTime, endTime] = request.getStartAndEndTime();
        result.append(std::format("{} {} {} {} {} {}\n",
                                  request.getNetwork().value_or("*"),
                                  request.getStation().value_or("*"),
                                  request.getLocationCode().value_or("--"),
                                  request.getChannel().value_or("*"),
                                  Details::toISO8601(startTime),
                                  Details::toISO8601(endTime)));
         
    }
    return result;
}

/// Iterators
BulkRequest::const_iterator BulkRequest::begin() const
{
    return pImpl->mRequests.cbegin();
}

BulkRequest::const_iterator BulkRequest::end() const
{
    return pImpl->mRequests.cend();
}

BulkRequest::const_iterator BulkRequest::cbegin() const
{
    return pImpl->mRequests.cbegin();
}

BulkRequest::const_iterator BulkRequest::cend() const
{
    return pImpl->mRequests.cend();
}


