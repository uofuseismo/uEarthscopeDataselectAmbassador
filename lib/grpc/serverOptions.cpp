#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ptree_fwd.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include "uEarthscopeDataselectAmbassador/grpc/serverOptions.hpp"
#include "secretFile.hpp"

using namespace UEarthscopeDataselectAmbassador::GRPC;


class ServerOptions::ServerOptionsImpl
{
public:
    std::string mHost{"localhost"};
    std::string mServerCertificate;
    std::string mServerKey;
    std::optional<std::string> mClientCertificate{std::nullopt};
    std::optional<std::string> mAccessToken{std::nullopt};
    int mMaximumRequestMessageSizeInBytes{65536};
    std::chrono::milliseconds mMaximumConnectionAge{std::chrono::seconds {180}};
    std::chrono::milliseconds mMaximumGracePeriod{std::chrono::seconds{2}};
    int mMaximumRequestIdentifierSize{128};
    uint16_t mPort{50000};
    bool mHaveServerCertificate{false}; 
    bool mHaveServerKey{false};
    bool mReflectionEnabled{false};
};


// Constructor
ServerOptions::ServerOptions() :
    pImpl(std::make_unique<ServerOptionsImpl> ())
{
}

/// Copy constructor
ServerOptions::ServerOptions(const ServerOptions &options)
{
    *this = options;
}

/// Move constructor
ServerOptions::ServerOptions(ServerOptions &&options) noexcept
{
    *this = std::move(options);
}

/// Copy assignment
ServerOptions&
ServerOptions::operator=(const ServerOptions &options)
{
    if (&options == this){return *this;}
    pImpl = std::make_unique<ServerOptionsImpl> (*options.pImpl);
    return *this;
}

/// Move assignment
ServerOptions&
ServerOptions::operator=(ServerOptions &&options) noexcept
{
    if (&options == this){return *this;}
    pImpl = std::move(options.pImpl);
    return *this;
}

/// Destructor
ServerOptions::~ServerOptions() = default;

/// Host
void ServerOptions::setHost(const std::string &hostIn)
{
    auto host = hostIn;
    host.erase(
       std::remove_if(host.begin(), host.end(),
                      [](unsigned char c)
                      {
                         return std::isspace(c);
                      }),
       host.end());
    if (host.empty())
    {
        throw std::invalid_argument("Host is empty");
    }
    pImpl->mHost = host;
}

std::string ServerOptions::getHost() const noexcept
{
    return pImpl->mHost;
}

std::string ServerOptions::getAddress() const noexcept
{
    auto address = getHost();
    return address.append(":").append(std::to_string(getPort()));
}

/// Port
void ServerOptions::setPort(const uint16_t port)
{
    if (port < 1)
    {
        throw std::invalid_argument("Port must be positive");
    }
    pImpl->mPort = port;
}

uint16_t ServerOptions::getPort() const noexcept
{
    return pImpl->mPort;
}

/// Server cert
void ServerOptions::setServerCertificate(const std::string &cert)
{
    if (cert.empty())
    {
        throw std::invalid_argument("Server certificate is empty");
    }
    pImpl->mHaveServerCertificate = true;
    pImpl->mServerCertificate = cert;
}

std::string ServerOptions::getServerCertificate() const
{
    if (!hasServerCertificate())
    {
        throw std::runtime_error("Server certificate not set");
    }
    return pImpl->mServerCertificate;
}

bool ServerOptions::hasServerCertificate() const noexcept
{
    return pImpl->mHaveServerCertificate;
}

void ServerOptions::setServerKey(const std::string &key)
{
    if (key.empty())
    {
        throw std::invalid_argument("Server key is empty");
    }
    pImpl->mHaveServerKey = true;
    pImpl->mServerKey = key;
}

std::string ServerOptions::getServerKey() const
{
    if (!hasServerKey())
    {
        throw std::runtime_error("Server key not set");
    }
    return pImpl->mServerKey;
}

bool ServerOptions::hasServerKey() const noexcept
{
    return pImpl->mHaveServerKey;
}

/// Client cert
void ServerOptions::setClientCertificate(const std::string &cert)
{
    if (cert.empty())
    {
        throw std::invalid_argument("Client certificate is empty");
    }
    pImpl->mClientCertificate = std::make_optional<std::string> (cert);
}

std::optional<std::string>
    ServerOptions::getClientCertificate() const noexcept
{
    return pImpl->mClientCertificate;
}

/// Reflection
void ServerOptions::enableReflection() noexcept
{
    pImpl->mReflectionEnabled = true;
}

void ServerOptions::disableReflection() noexcept
{
    pImpl->mReflectionEnabled = false;
}

bool ServerOptions::isReflectionEnabled() const noexcept
{
    return pImpl->mReflectionEnabled;
}


/// Access token
void ServerOptions::setAccessToken(const std::string &token)
{
    if (token.empty()){throw std::invalid_argument("Token is empty");}
    pImpl->mAccessToken = std::make_optional<std::string> (token);
}

std::optional<std::string> ServerOptions::getAccessToken() const noexcept
{
    return pImpl->mAccessToken;
}

/// Maximum connection age
void ServerOptions::setMaximumConnectionAge(
    const std::chrono::milliseconds &duration)
{
    if (duration.count() < 1)
    {
        throw std::invalid_argument("Max connection duration must be positive");
    }
    pImpl->mMaximumConnectionAge = duration;
}

std::chrono::milliseconds
ServerOptions::getMaximumConnectionAge() const noexcept
{
    return pImpl->mMaximumConnectionAge;
}

/// Maximum grace period
void ServerOptions::setMaximumConnectionAgeGracePeriod(
    const std::chrono::milliseconds &period)
{
    if (period.count() < 0)
    {
        throw std::invalid_argument("Grace period must be non-negative");
    }
    pImpl->mMaximumGracePeriod = period;
}

std::chrono::milliseconds
ServerOptions::getMaximumConnectionAgeGracePeriod() const noexcept
{
    return pImpl->mMaximumGracePeriod;
}

/// Maximum request message
void ServerOptions::setMaximumRequestMessageSizeInBytes(
    const int maxMessageSize)
{
    if (maxMessageSize < 1)
    {
        throw std::invalid_argument(
            "Max request message size must be positive");
    }
    pImpl->mMaximumRequestMessageSizeInBytes = maxMessageSize;
}

int ServerOptions::getMaximumRequestMessageSizeInBytes() const noexcept
{
    return pImpl->mMaximumRequestMessageSizeInBytes;
}

/// Max request identifier size
void ServerOptions::setMaximumRequestIdentifierSizeInBytes(
    const int maxIdentifierSize)
{
    if (maxIdentifierSize < 1 || maxIdentifierSize > 8192)
    {
        throw std::invalid_argument(
           "Max request identifier size must be in range [1,8192]");
    }
    pImpl->mMaximumRequestIdentifierSize = maxIdentifierSize;
}

int ServerOptions::getMaximumRequestIdentifierSizeInBytes() const noexcept
{
    return pImpl->mMaximumRequestIdentifierSize;
}

namespace
{
template<typename T>
T getOrDefault(const boost::property_tree::ptree &tree,
               const std::string &key,
               const T &defaultValue)
{
    const auto text = tree.get_optional<std::string> (key);
    if (!text){return defaultValue;}          // Not in the file
    const auto value = tree.get_optional<T> (key);
    if (!value)                               // In the file but garbage
    {
        throw std::invalid_argument(key + " = " + *text
                                  + " is not a valid value");
    }
    return *value;
}
}

ServerOptions ServerOptions::fromInitializationFile(
    const std::filesystem::path &iniFile,
    const std::string &sectionIn)
{
    using namespace UEarthscopeDataselectAmbassador;
    if (!std::filesystem::exists(iniFile))
    {
        throw std::invalid_argument("Initialization file "
                                  + iniFile.string() + " does not exist");
    }
    ServerOptions options;
    // Make sure section ends with . so we can find stuff
    auto section = sectionIn;
    if (!section.empty() && section.back() != '.'){section.append(".");}

    // Parse the initialization file
    boost::property_tree::ptree propertyTree;
    boost::property_tree::ini_parser::read_ini(iniFile, propertyTree);

    auto host
        = propertyTree.get<std::string> (section + "host",
                                         options.getHost());
    if (host.empty())
    {
        throw std::invalid_argument(section + "host is empty");
    }
    options.setHost(host);

    int port{50000};
    port = ::getOrDefault<int> (propertyTree,
                                section + "port",
                                static_cast<int> (port));
    if (port < 1 || port > std::numeric_limits<uint16_t>::max())
    {
        throw std::invalid_argument(section + "port is out of range [1,65535]");
    }
    options.setPort(port);

    auto serverKey
        = resolveSecret(propertyTree,
                          section + "serverKey",
                          section + "serverKeyFile");
    auto serverCertificate
        = resolveSecret(propertyTree,
                          section + "serverCertificate",
                          section + "serverCertificateFile");
    bool haveServerCerts{false};
    if (serverKey != std::nullopt && serverCertificate != std::nullopt)
    {
        options.setServerKey(*serverKey);
        options.setServerCertificate(*serverCertificate);
        haveServerCerts = true;
    }
    else if (serverKey != std::nullopt || serverCertificate != std::nullopt)
    {
        throw std::invalid_argument(
           "Server key and certificate must both be set");
    }

    auto clientCertificate
        = resolveSecret(propertyTree,
                          section + "clientCertificate",
                          section + "clientCertificateFile");
    if (clientCertificate != std::nullopt)
    {
        if (!haveServerCerts)
        {
            throw std::invalid_argument(
               "Client cert requires server cert and server key be set");
        }
        options.setClientCertificate(*clientCertificate);
    }

    auto accessToken
        = resolveSecret(propertyTree,
                          section + "accessToken",
                          section + "accessTokenFile");
    if (accessToken != std::nullopt)
    {
        if (!haveServerCerts)
        {
            throw std::invalid_argument(
               "Access token requires server cert and server key be set");
        }

        options.setAccessToken(*accessToken); 
    }
 
    auto enableReflection
        = ::getOrDefault<bool> (propertyTree,
                                 section + "enableReflection",
                                 false);
    options.disableReflection();
    if (enableReflection){options.enableReflection();}

    auto maximumRequestMessageSize
        = ::getOrDefault<int> (propertyTree,
                               section + "maximumRequestMessageSizeInBytes",
                               options.getMaximumRequestMessageSizeInBytes());
    options.setMaximumRequestMessageSizeInBytes(maximumRequestMessageSize);

    auto maximumRequestIdentifierSize
        = ::getOrDefault<int> (propertyTree,
                              section + "maximumRequestIdentifierSizeInBytes",
                              options.getMaximumRequestIdentifierSizeInBytes());
    options.setMaximumRequestIdentifierSizeInBytes(
        maximumRequestIdentifierSize);

    int maxConnectionAgeMilliS{
       static_cast<int> (options.getMaximumConnectionAge().count())};
    maxConnectionAgeMilliS
         = ::getOrDefault<int> (propertyTree,
                                section + "maximumConnectionAgeInMilliSeconds",
                                maxConnectionAgeMilliS);
    options.setMaximumConnectionAge(
        std::chrono::milliseconds(maxConnectionAgeMilliS));

    int maxGracePeriodMilliS{
        static_cast<int> (options.getMaximumConnectionAgeGracePeriod().count())};
    maxGracePeriodMilliS
          = ::getOrDefault<int> (
                propertyTree,
                section + "maximumConnectionAgeGracePeriodInMilliSeconds",
                maxGracePeriodMilliS);
    options.setMaximumConnectionAgeGracePeriod(
        std::chrono::milliseconds(maxGracePeriodMilliS));

    return options;
}
