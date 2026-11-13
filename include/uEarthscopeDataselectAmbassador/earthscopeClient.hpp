#ifndef UEARTHSCOPE_DATASELECT_AMBASSADOR_EARTHSCOPE_CLIENT_HPP
#define UEARTHSCOPE_DATASELECT_AMBASSADOR_EARTHSCOPE_CLIENT_HPP
#include <memory> 
#include <spdlog/logger.h>
namespace UEarthscopeDataselectAmbassador
{
 class EarthscopeClientOptions;
}
namespace UEarthscopeDataselectAmbassador
{   
/// @class EarthscopeClient
/// @brief The client that actually cURLs Earthscope's dataselect API.
/// @copyright Ben Baker (University of Utah) distributed under the
///            MIT NO AI license.
class EarthscopeClient
{   
public:
    /// @brief Constructor.
    /// @throws std::invalid_argument if the options lack an access token and
    ///         the legacy credentials.
    EarthscopeClient(const EarthscopeClientOptions &options,
                     std::shared_ptr<spdlog::logger> logger);

    /// @brief True indicates that the class is initialized.
    [[nodiscard]] bool isInitialized() const noexcept;

    /// @brief Destructor
    ~EarthscopeClient();
    EarthscopeClient() = delete;
    EarthscopeClient(const EarthscopeClient &) = delete;
    EarthscopeClient(EarthscopeClient &&) noexcept = delete;
    EarthscopeClient& operator=(const EarthscopeClient &) = delete;
    EarthscopeClient& operator=(EarthscopeClient &&) noexcept = delete;
    
private:
    class EarthscopeClientImpl;
    std::unique_ptr<EarthscopeClientImpl> pImpl;
};
}
#endif
