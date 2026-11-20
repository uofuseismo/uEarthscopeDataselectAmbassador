#ifndef UEARTHSCOPE_DATASELECT_AMBASSADOR_EARTHSCOPE_CLIENT_HPP
#define UEARTHSCOPE_DATASELECT_AMBASSADOR_EARTHSCOPE_CLIENT_HPP
#include <expected>
#include <memory> 
#include <spdlog/logger.h>
namespace UEarthscopeDataselectAmbassador
{
 class BulkRequest;
 class CURLGlobal;
 class EarthscopeClientOptions;
 class Request;
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
    struct Response
    {
        std::string payload;     /*!< The response. */
        std::string contentType; /*!< Content type code. */ 
        int status;              /*!< The HTTP status code. */
    };
 
    enum class Error
    {
        EmptyURL,          /*!< The URL is empty. */
        InvalidRequest,    /*!< Requisite information in the request was missing
                                such as the start and end time. */
        ConnectionFailure, /*!< Earthscope connection failure - try again later. */
        TimedOut,          /*!< Request timed out. */
        TooLarge,          /*!< The resulting payload was too large, retrying
                                won't help here. */
        ServerError        /*!< An internal server error. */ 
    };
public:
    /// @brief Constructor.
    /// @param[in] options     The earthscope client options.
    /// @param[in] curlGlobal  This is the proof that curl global init
    ///                        was called.
    /// @param[in] logger      A handle to the  application logger.
    /// @throws std::invalid_argument if the options lack an access token and
    ///         the legacy credentials.
    EarthscopeClient(const EarthscopeClientOptions &options,
                     const CURLGlobal &curlGlobal,
                     std::shared_ptr<spdlog::logger> logger);

    /// @brief True indicates that the class is initialized.
    [[nodiscard]] bool isInitialized() const noexcept;

    /// @brief Performs a GET request to the Earthscope FDSNWS.
    [[nodiscard]] auto get(const Request &request) const -> std::expected<Response, Error>;
    /// @brief Performs a POST request to the Earthscope FDSNWS.
    [[nodiscard]] auto post(const BulkRequest &request) const -> std::expected<Response, Error>;

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
