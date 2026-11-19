#ifndef UEARTHSCOPE_DATASELECT_AMBASSADOR_BULK_REQUEST_HPP
#define UEARTHSCOPE_DATASELECT_AMBASSADOR_BULK_REQUEST_HPP
#include <memory>
#include <vector>
#include <uEarthscopeDataselectAmbassador/request.hpp>

namespace UEarthscopeDataselectAmbassador
{
/// @class BulkRequest bulkRequest.hpp
/// @brief A bulk request is a container for many individual requests.
/// @copyright Ben Baker (University of Utah) distributed under the
///            MIT NO AI license.
class BulkRequest
{
public:

public:
    /// @brief Constructor.
    BulkRequest();
    /// @brief Copy constructor.
    BulkRequest(const BulkRequest &request);
    /// @brief Move constructor.
    BulkRequest(BulkRequest &&request) noexcept;
    /// @brief Constructs from a POST string. 
    explicit BulkRequest(const std::string &body);

    /// @brief A list of requests 
//    [[nodiscard]] const std::vector<Request> &getRequests() const noexcept;
    /// @result The data format.
    [[nodiscard]] Request::DataFormat getDataFormat() const noexcept;
    /// @result True inidicates that no data being found is a 404 error.
    ///         Otherwise no data is a 204 error.
    [[nodiscard]] bool noDataIs404() const noexcept;
    /// @result The bulk request to submit to Earthscope. 
    [[nodiscard]] std::string toString() const;

    /// @brief Destructor.
    ~BulkRequest();
    /// @brief Copy assignment.
    BulkRequest& operator=(const BulkRequest &request);
    /// @brief Move assignment.
    BulkRequest& operator=(BulkRequest &&request) noexcept; 
private:
    class BulkRequestImpl;
    std::unique_ptr<BulkRequestImpl> pImpl;
};
}
#endif
