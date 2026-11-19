#include <array>
#include <memory>
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

using namespace UEarthscopeDataselectAmbassador;

namespace
{
constexpr int MAXIMUM_SELECTIONS{500};

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
        if (const auto equals = line.find('='); equals != std::string::npos)
        {
            auto key = boost::algorithm::trim_copy(line.substr(0, equals));
            if (::isRetired(key)){continue;}
            if (!boost::iequals(key, "format") && !boost::iequals(key, "nodata"))
            {
                throw std::invalid_argument("Unsupported POST option");
            }
            options.append("&" + line);
            continue;
        }
        if (line.find_first_of("&%") != std::string::npos)
        {
            throw std::invalid_argument("Invalid character in selection");
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
