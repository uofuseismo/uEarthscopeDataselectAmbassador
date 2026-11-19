#ifndef UEARTHSCOPE_DATASELECT_AMBASSADOR_REQUEST_HPP
#define UEARTHSCOPE_DATASELECT_AMBASSADOR_REQUEST_HPP
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace UEarthscopeDataselectAmbassador
{

/// @class Request request.hpp
/// @brief Defines a request to Earthscope's data select.
/// @copyright Ben Baker (University of Utah) distributed under the
///            MIT NO AI license.
class Request
{
public:
    /// @brief Defines the output data format.
    enum class DataFormat
    {
        MiniSEED,      /*!< Request miniSEED data - miniseed. */
        GeoCSV,        /*!< GeoCSV - geocsv. */
        GeoCSV_INLINE, /*!< GeoCSV inline - geocsv.inline. */
        GeoCSV_ZIP,    /*!< GeoCSV ZIP - geocsv.zip. */
        GeoCSV_TSPAIR, /*!< GeoCSV TSPair - geocsv.tspair. */
        GeoCSV_SLIST   /*!< GeoCSV SList - geocsv.slists. */ 
    };

public:
    /// @brief Constructor.
    Request();
    /// @brief Copy constructor.
    Request(const Request &request);
    /// @brief Move constructor.
    Request(Request &&request) noexcept;
    /// @brief Constructs from a query string.
    explicit Request(const std::string &query);

    /// @brief Sets the start and time of the query.
    /// @param[in] startAndEndTime  The start and end time (UTC) of the query
    ///                             measured in microseconds since the epoch (1970).
    /// @throws std::invalid_argument if the end time is not greater than the
    ///         start time.
    void setStartAndEndTime(const std::pair<std::chrono::microseconds,
                                            std::chrono::microseconds> &startAndTime);
    /// @result The start and end time (UTC).
    /// @throws std::runtime_error if \c hasStartAndTime() is false.
    [[nodiscard]] std::pair<std::chrono::microseconds, std::chrono::microseconds>
        getStartAndEndTime() const;
    /// @result True indicates that the start and time were set.
    [[nodiscard]] bool hasStartAndEndTime() const noexcept;

    /// @brief Sets the network code - e.g., UU.
    void setNetwork(const std::string &network);
    /// @result The network code.
    [[nodiscard]] std::optional<std::string> getNetwork() const noexcept;

    /// @brief Sets the station name - e.g., CWU.
    void setStation(const std::string &station);
    /// @result The station name.
    [[nodiscard]] std::optional<std::string> getStation() const noexcept;

    /// @brief Sets the channel code - e.g., HHZ or HH?
    void setChannel(const std::string &channel);
    /// @result The channel code.
    [[nodiscard]] std::optional<std::string> getChannel() const noexcept;

    /// @brief Sets the location code - e.g., 01.
    /// @note If this is empty or "  " then it will be changed to "--". 
    void setLocationCode(const std::string &location);
    /// @result The location code.
    [[nodiscard]] std::optional<std::string> getLocationCode() const noexcept;

    /// @brief Sets the desired data format.
    void setDataFormat(DataFormat format) noexcept;
    /// @result The desired data format.  
    /// @note By default this is miniSEED.
    [[nodiscard]] DataFormat getDataFormat() const noexcept;

    /// @brief Enables no data found being a 404 error.
    void enableNoDataIs404() noexcept;
    /// @brief Disables no data found being a 404 error - result is 204 instead.
    void disableNoDataIs404() noexcept;
    /// @result True indicates that no data is a 404 error.
    /// @note By default this is false - i.e., no data is a 204 error.
    [[nodiscard]] bool noDataIs404() const noexcept;

    /// @result The parameters to append to the service.earthscope.org/fdsnws/dataselect/1/query[auth]? base URL 
    [[nodiscard]] std::string toString() const;

    /// @brief Destructor.
    ~Request();
    /// @brief Copy assignment.
    Request& operator=(const Request &request);
    /// @brief Move assignment.
    Request& operator=(Request &&request) noexcept;
private:
    class RequestImpl;
    std::unique_ptr<RequestImpl> pImpl; 
};

}
#endif
