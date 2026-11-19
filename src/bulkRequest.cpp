#include <memory>
#include <utility>
#include "uEarthscopeDataselectAmbassador/bulkRequest.hpp"
#include "uEarthscopeDataselectAmbassador/request.hpp"

using namespace UEarthscopeDataselectAmbassador;

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
