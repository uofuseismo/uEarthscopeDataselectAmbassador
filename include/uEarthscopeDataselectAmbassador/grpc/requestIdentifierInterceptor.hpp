#ifndef UEARTHSCOPE_DATASELECT_AMBASSADOR_GRPC_REQUEST_IDENTIFIER_INTERCEPTOR_HPP
#define UEARTHSCOPE_DATASELECT_AMBASSADOR_GRPC_REQUEST_IDENTIFIER_INTERCEPTOR_HPP
#include <cstddef>
#include <string>
#include <grpcpp/support/server_interceptor.h>

namespace UEarthscopeDataselectAmbassador::GRPC
{

/// @class RequestIdentifierInterceptor "reuestIdentifierInterceptor.hpp"
/// @brief Provides a mechanism to check incoming requests for a identifier 
///        in the x-request-id HTTP header and, if it is not specified or
///        too long, inject a UID.
/// @copyright Ben Baker (University of Utah) distributed under the 
///            MIT NO AI license.
class RequestIdentifierInterceptor : public grpc::experimental::Interceptor
{
public:
    explicit RequestIdentifierInterceptor(size_t maximumIdentifierSize);
    void Intercept(grpc::experimental::InterceptorBatchMethods *methods) override;
    RequestIdentifierInterceptor() = delete;
private:
    std::string mIdentifier;
    size_t mMaximumIdentifierSize{128};
};

}
#endif
