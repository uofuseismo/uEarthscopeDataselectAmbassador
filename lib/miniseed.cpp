#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <exception>
#include <stdexcept>
#include <string>
#include <vector>
#include <libmseed.h>
#include "uEarthscopeDataselectAmbassador/miniseed.hpp"

using namespace UEarthscopeDataselectAmbassador::MiniSEED;

namespace
{

Packet fromMiniSEEDRecord(const MS3Record &miniSEEDRecord)
{
    Packet packet;
    packet.startTime = std::chrono::nanoseconds {miniSEEDRecord.starttime};
    packet.samplingRate = miniSEEDRecord.samprate;
    packet.nSamples = static_cast<int> (miniSEEDRecord.samplecnt);
    if (miniSEEDRecord.sampletype == 'i')
    {
        packet.dataType = Packet::DataType::Int32;
    }
    else if (miniSEEDRecord.sampletype == 'f')
    {
        packet.dataType = Packet::DataType::Float32;
    }
    else if (miniSEEDRecord.sampletype == 'd')
    {
        packet.dataType = Packet::DataType::Float64;
    }
    else if (miniSEEDRecord.sampletype == 't')
    {
        packet.dataType = Packet::DataType::Text;
    }
    else
    {
        throw std::runtime_error("Unhandled packet type "
                               + std::to_string(miniSEEDRecord.sampletype));
    }
}

}

std::vector<Packet> 
UEarthscopeDataselectAmbassador::MiniSEED::extract(
    unsigned char *payload,
    const int payloadSize,
    const Selection &selection)
{
    if (payload == nullptr)
    {
        throw std::invalid_argument("Payload is null");
    }
    std::vector<Packet> packets;
    if (payloadSize < 1){return packets;}

    constexpr uint16_t nslc2sidFlags{0};
    std::array<char, LM_SIDLEN> sid;
    if (ms_nslc2sid(sid.data(), sid.size(),
                    nslc2sidFlags,
                    selection.network.data(),
                    selection.station.data(),
                    selection.locationCode.data(),
                    selection.channel.data()) < 0)
    {
        throw std::runtime_error("Failed to make SID");
    }

    constexpr nstime_t startTime{NSTUNSET};
    constexpr nstime_t endTime{NSTUNSET};
    constexpr uint8_t publicationVersion{0}; // Any
    MS3Selections *selections;
    ms3_addselect(&selections, sid.data(), startTime, endTime, publicationVersion);


    auto buffer = reinterpret_cast<char *> (payload);
    auto bufferLength = static_cast<uint64_t> (payloadSize);
    uint64_t offset{0};
    // Iterate through the consumed buffer
    while (bufferLength - offset > MINRECLEN)
    {
        // Defer unpacking data
        MS3Record *miniSEEDRecord{nullptr};
        constexpr uint32_t parseFlags = ~MSF_UNPACKDATA;
        constexpr int8_t verbose{0};
        // Convert every packet in the buffer
        auto parseValue
            = msr3_parse_next(buffer, bufferLength, &offset,
                              &miniSEEDRecord, parseFlags, verbose);
        if (parseValue == 0)
        {
            // Is the selection we're looking for?
            if (ms3_matchselect(selections,
                                miniSEEDRecord->sid,
                                miniSEEDRecord->starttime,
                                msr3_endtime(miniSEEDRecord),
                                miniSEEDRecord->pubversion, nullptr))
            {
                // Unpack it
                if (msr3_unpack_data(miniSEEDRecord, verbose) !=
                                     miniSEEDRecord->samplecnt)
                {
                    if (miniSEEDRecord != nullptr)
                    {
                        msr3_free(&miniSEEDRecord);
                        miniSEEDRecord = nullptr;
                    }
                    ms3_freeselections(selections);
                    selections = nullptr;
                    throw std::runtime_error("Failed to unpack data in packet");
                }
                try
                {
                    packets.push_back(::fromMiniSEEDRecord(*miniSEEDRecord));
                }
                catch (const std::exception &e)
                {
                    if (miniSEEDRecord != nullptr)
                    {
                        msr3_free(&miniSEEDRecord);
                        miniSEEDRecord = nullptr;
                    }
                    ms3_freeselections(selections);
                    selections = nullptr;
                    throw;
                }
            }
            // Hit or miss move forward
            offset = offset + miniSEEDRecord->reclen;
        }
        else if (parseValue > 0)
        {
            if (miniSEEDRecord != nullptr)
            {
                msr3_free(&miniSEEDRecord);
                miniSEEDRecord = nullptr;
            }
            ms3_freeselections(selections);
            selections = nullptr;
            break;
        }
        else //if (parsevalue < 0)
        {
            if (miniSEEDRecord != nullptr)
            {
                msr3_free(&miniSEEDRecord);
            }
            ms3_freeselections(selections);
            throw std::runtime_error("miniSEED error unpacking data");
        }
    }
    std::sort(packets.begin(), packets.end(),
              [](const auto &lhs, const auto &rhs)
              {
                  return lhs.startTime <= rhs.startTime;
              });
    return packets;
}

