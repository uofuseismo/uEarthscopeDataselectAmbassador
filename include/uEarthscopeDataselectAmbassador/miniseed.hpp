#ifndef UEARTHSCOPE_DATASELECT_AMBASSADOR_MINISEED_HPP
#define UEARTHSCOPE_DATASELECT_AMBASSADOR_MINISEED_HPP
#include <chrono>
#include <string>
#include <vector>
namespace UEarthscopeDataselectAmbassador::MiniSEED
{

/// @brief Defines a record selection by NSCL.
struct Selection
{
    std::string network;
    std::string station;
    std::string channel;
    std::string locationCode;
};

/// @brief Defines a packet.
struct Packet
{
    enum class DataType
    {
        Text,
        Int32,
        Float32,
        Float64
    };
    std::vector<unsigned char> data;
    std::chrono::nanoseconds startTime{0};
    double samplingRate{0}; 
    int nSamples{0};
    DataType dataType{DataType::Int32};
};

/// @brief Extracts packets for a selectio from a miniSEED file.
/// @param[in] payload      The payload received from the cURL request.
/// @param[in] payloadSize  The size of the payload. 
std::vector<Packet> extract(unsigned char *payload, const int payloadSize,
                            const Selection &selection);

                             
}
#endif
