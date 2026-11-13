#include <algorithm>
#include <cctype>
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

using namespace UEarthscopeDataselectAmbassador;

namespace
{
[[nodiscard]] 
std::string transform(const std::string &nameIn)
{
    const auto isSpace = [](const unsigned char c)
                         {
                             return std::isspace(c) != 0;
                         };
    auto first = std::ranges::find_if_not(nameIn, isSpace);
    auto last = std::ranges::find_if_not(nameIn.rbegin(), nameIn.rend(),
                                         isSpace).base();
    if (first >= last){return {};} 
    std::string name{first, last};
    for (auto &c : name)
    {    
        const auto lowered
            = static_cast<char> (std::tolower(static_cast<unsigned char> (c)));
        const auto isAllowed = (lowered >= 'a' && lowered <= 'z') 
                            || (lowered >= '0' && lowered <= '9') 
                            || lowered == '.' || lowered == '_'
                            || lowered == '-' || lowered == '@'; 
        if (!isAllowed){return {};} 
        c = lowered;
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
    auto user = ::transform(userNameAndPassword.first);
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
    return options;
}   

