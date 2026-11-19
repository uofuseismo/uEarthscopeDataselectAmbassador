#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <boost/algorithm/string/trim.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ptree_fwd.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include "uEarthscopeDataselectAmbassador/earthscopeClientOptions.hpp"
#include "secretFile.hpp"

namespace
{
constexpr int MAXIMUM_PAYLOAD_MB{32};
constexpr int REQUEST_TIMEOUT_S{15};
constexpr int CONNECT_TIMEOUT_S{5};
constexpr int MAXIMUM_REQUESTS_PER_SECOND{500};
}

using namespace UEarthscopeDataselectAmbassador;

namespace
{

[[nodiscard]]
std::string sanitizeUserName(const std::string &nameIn)
{
    std::string name;
    name.reserve(nameIn.size());
    for (const auto c : nameIn)
    {
        if (c == '\r' || c == '\n' || c == ':' || c == ' ' || c == '\t')
        {
            continue;
        }
        name.push_back(c);
    }
    return name;
}
}


class EarthscopeClientOptions::EarthscopeClientOptionsImpl
{
public:
    std::pair<std::string, std::string> mUserNameAndPassword;
    std::string mAccessToken;
    std::string mEndpoint{"https://service.earthscope.org/fdsnws/dataselect/1"};
    int64_t mMaximumPayloadInBytes
    {
        MAXIMUM_PAYLOAD_MB*1024L*1024L
    };
    std::chrono::milliseconds mRequestTimeOut
    {
        std::chrono::seconds {REQUEST_TIMEOUT_S}
    };
    std::chrono::milliseconds mConnectTimeOut
    {
        std::chrono::seconds {CONNECT_TIMEOUT_S}
    };
    int mMaximumRequestsPerSecond{MAXIMUM_REQUESTS_PER_SECOND};
    bool mHasUserNameAndPassword{false}; 
};

/// Constructor
EarthscopeClientOptions::EarthscopeClientOptions() :
    pImpl(std::make_unique<EarthscopeClientOptionsImpl> ())
{
}

/// Copy constructor
EarthscopeClientOptions::EarthscopeClientOptions(
    const EarthscopeClientOptions &options)
{
    *this = options;
}

/// Copy assignment
EarthscopeClientOptions::EarthscopeClientOptions(
    EarthscopeClientOptions &&options) noexcept
{
    *this = std::move(options);
}

/// Copy assignment
EarthscopeClientOptions&
EarthscopeClientOptions::operator=(const EarthscopeClientOptions &options)
{
    if (&options == this){return *this;}
    pImpl = std::make_unique<EarthscopeClientOptionsImpl> (*options.pImpl);
    return *this;
}

/// Move assignment
EarthscopeClientOptions&
EarthscopeClientOptions::operator=(EarthscopeClientOptions &&options) noexcept
{
    if (&options == this){return *this;}
    pImpl = std::move(options.pImpl);
    return *this;
}

/// Token
void EarthscopeClientOptions::setAccessToken(const std::string &token)
{
    if (token.empty())
    {
        throw std::invalid_argument("Token is empty");
    }
    pImpl->mAccessToken = token;
}

std::string EarthscopeClientOptions::getAccessToken() const
{
    if (!hasAccessToken())
    {
        throw std::runtime_error("Access token not set");
    }
    return pImpl->mAccessToken;
}

bool EarthscopeClientOptions::hasAccessToken() const noexcept
{
    return !pImpl->mAccessToken.empty();
}

/// User and password
void EarthscopeClientOptions::setLegacyCredentials(
    const std::pair<std::string, std::string> &userNameAndPassword)
{
    auto user = ::sanitizeUserName(userNameAndPassword.first);
    if (user.empty()){throw std::invalid_argument("User name is empty");}
    auto password = userNameAndPassword.second;
    if (password.empty())
    {
        throw std::invalid_argument(" ");
    }
    pImpl->mUserNameAndPassword
         = std::pair{std::move(user), std::move(password)}; 
    pImpl->mHasUserNameAndPassword = true;
}

std::pair<std::string, std::string> 
EarthscopeClientOptions::getLegacyCredentials() const
{
    if (!hasLegacyCredentials())
    {
        throw std::runtime_error("Legacy credentials not set");
    }
    return pImpl->mUserNameAndPassword;
}

bool EarthscopeClientOptions::hasLegacyCredentials() const noexcept
{
    return pImpl->mHasUserNameAndPassword;
}

/// Payload
void EarthscopeClientOptions::setMaximumResponseSizeInBytes(
    const int64_t maximumResponseInBytes)
{
    if (maximumResponseInBytes < 1)
    {
        throw std::invalid_argument("Maximum response size must be positive");
    }
    pImpl->mMaximumPayloadInBytes = maximumResponseInBytes;
}

int64_t EarthscopeClientOptions::getMaximumResponseSizeInBytes()
    const noexcept
{
    return pImpl->mMaximumPayloadInBytes;
}

/// Request time out
void EarthscopeClientOptions::setRequestTimeOut(
    const std::chrono::milliseconds &timeOut)
{
    if (timeOut <= std::chrono::milliseconds {0})
    {
        throw std::invalid_argument("Time out must be positive");
    }
    pImpl->mRequestTimeOut = timeOut;
}

std::chrono::milliseconds 
EarthscopeClientOptions::getRequestTimeOut() const noexcept
{
    return pImpl->mRequestTimeOut;
}

/// Connect time out
void EarthscopeClientOptions::setConnectTimeOut(
    const std::chrono::milliseconds &timeOut)
{
    if (timeOut <= std::chrono::milliseconds {0})
    {   
        throw std::invalid_argument("Connect time out must be positive");
    }
    pImpl->mConnectTimeOut = timeOut;
}

std::chrono::milliseconds 
EarthscopeClientOptions::getConnectTimeOut() const noexcept
{
    return pImpl->mConnectTimeOut;
}

/// Endpoint
void EarthscopeClientOptions::setEndpoint(const std::string &endpointIn)
{
    auto endpoint = boost::algorithm::trim_copy(endpointIn); 
    if (endpoint.empty())
    {
        throw std::invalid_argument("Endpoit is empty");
    }
    pImpl->mEndpoint = endpoint;
}

std::string EarthscopeClientOptions::getEndpoint() const
{
    return pImpl->mEndpoint;
}

/// RPS
void EarthscopeClientOptions::setMaximumRequestsPerSecond(
    const int maxRequestsPerSecond)
{
    if (maxRequestsPerSecond < 1)
    {
        throw std::invalid_argument(
            "Maximum requests per second must be positive");
    }
    pImpl->mMaximumRequestsPerSecond = maxRequestsPerSecond;
}

int EarthscopeClientOptions::getMaximumRequestsPerSecond() const noexcept
{
    return pImpl->mMaximumRequestsPerSecond;
}

/// Destructor
EarthscopeClientOptions::~EarthscopeClientOptions() = default;

/// Create from options
EarthscopeClientOptions
EarthscopeClientOptions::fromInitializationFile(
    const std::filesystem::path &iniFile,
    const std::string &sectionIn)
{
    if (!std::filesystem::exists(iniFile))
    {
        throw std::invalid_argument(
           std::string {iniFile}.append(" does not exist"));
    }
    boost::property_tree::ptree propertyTree;
    boost::property_tree::ini_parser::read_ini(iniFile, propertyTree);

    EarthscopeClientOptions options;
    auto section = sectionIn;
    if (section.empty()){section = "Earthscope";}
    if (section.back() != '.'){section.append(".");}
    options.setEndpoint(
         propertyTree.get<std::string> (section + "endpoint",
                                        options.getEndpoint()));
    // Prefer the token
    auto accessToken
         = ::resolveSecret(propertyTree,
                           section + "accessToken",
                           section + "accessTokenFile");
    if (accessToken != std::nullopt)
    {
        options.setAccessToken(*accessToken);
    }

    // Try the old-school creds
    if (!options.hasAccessToken())
    {
        auto userName = ::resolveSecret(propertyTree,
                           section + "user",
                           section + "userFile");
        auto password = ::resolveSecret(propertyTree,
                           section + "password",
                           section + "passwordFile");
        if (userName != std::nullopt && password != std::nullopt)
        {
            options.setLegacyCredentials(std::pair {*userName, *password});
        }
    }
    if (!options.hasAccessToken() && !options.hasLegacyCredentials())
    {
        throw std::invalid_argument("Access token or basic creds required");
    }

    // Timeouts
    auto requestTimeOut
        = propertyTree.get<int> 
          (section + "requestTimeOutInSeconds",
           REQUEST_TIMEOUT_S);
    options.setRequestTimeOut(std::chrono::seconds {requestTimeOut}); 

    auto connectTimeOut
        = propertyTree.get<int> 
          (section + "connectTimeOutInSeconds",
           CONNECT_TIMEOUT_S);
    options.setConnectTimeOut(std::chrono::seconds {connectTimeOut});

    // Maximum payload
    int64_t maximumPayload
        = propertyTree.get<int>
          (section + "maximumResponseSizeInMegabytes",
           MAXIMUM_PAYLOAD_MB);
    maximumPayload = maximumPayload*(1024L*1024L);
    options.setMaximumResponseSizeInBytes(maximumPayload);

    // RPS
    options.setMaximumRequestsPerSecond(
        propertyTree.get<int>
        (section + "maximumRequestsPerSecond",
        MAXIMUM_REQUESTS_PER_SECOND));
    return options;
}   

