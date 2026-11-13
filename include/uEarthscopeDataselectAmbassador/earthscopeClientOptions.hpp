#ifndef UEARTHSCOPE_DATASELECT_AMBASSADOR_EARTHSCOPE_CLIENT_OPTIONS_HPP
#define UEARTHSCOPE_DATASELECT_AMBASSADOR_EARTHSCOPE_CLIENT_OPTIONS_HPP
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
namespace UEarthscopeDataselectAmbassador
{
/// @class EarthscopeClientOptions
/// @brief Defines the options for cURL'ing Earthscope's dataselect API.
/// @copyright Ben Baker (University of Utah) distributed under the
///            MIT NO AI license.
class EarthscopeClientOptions
{
public:
    /// @brief Constructor.
    EarthscopeClientOptions();
    /// @brief Copy constructor.
    EarthscopeClientOptions(const EarthscopeClientOptions &options);
    /// @brief Move constructor.
    EarthscopeClientOptions(EarthscopeClientOptions &&options) noexcept;

    /// @brief Sets the access token.
    /// @throws std::invalid_argument if the token is empty.
    /// @note This takes precedence over the username and password since
    ///       Earthscope wants to deprecate basic credentials.
    void setAccessToken(const std::string &token);
    /// @result The token.
    /// @throws std::runtime_error if \c hasAccessToken() is false.
    [[nodiscard]] std::string getAccessToken() const;
    /// @result True indicates that the access token was set.
    [[nodiscard]] bool hasAccessToken() const noexcept;

    /// @brief Sets the basic credentials for accessing dataselect.
    /// @throws std::invalid_argument if the user name or password is empty.
    void setLegacyCredentials(const std::pair<std::string, std::string> &userNameAndPassword);
    /// @result The user name and password.
    /// @throws std::runtime_error if \c hasLegacyCredentials() is false.
    [[nodiscard]] std::pair<std::string, std::string> getLegacyCredentials() const;
    /// @result True indicates that the basic credentials were set.
    [[nodiscard]] bool hasLegacyCredentials() const noexcept;

    /// @brief A way to override the endoint.   
    void setEndpoint(const std::string &endpoint);
    /// @result The earthscope endpoint.  
    /// @note By default this is https://service.earthscope.org/fdsnws/dataselect/1
    [[nodiscard]] std::string getEndpoint() const;

    /// @brief Creates the earthscope client options from an
    ///        initialization file.
    [[nodiscard]] static EarthscopeClientOptions 
        fromInitializationFile(const std::filesystem::path &iniFile,
                               const std::string &section = "Earthscope");

    /// @brief Copy assignment.
    EarthscopeClientOptions& operator=(const EarthscopeClientOptions &);
    /// @brief Move assignment.
    EarthscopeClientOptions& operator=(EarthscopeClientOptions &&) noexcept;
    /// @brief Destructor.
    ~EarthscopeClientOptions();
private:
    class EarthscopeClientOptionsImpl;
    std::unique_ptr<EarthscopeClientOptionsImpl> pImpl; 
}; 
}
#endif
