#include <chrono>
#include <stdexcept>
#include <string>
#include <utility>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include "uEarthscopeDataselectAmbassador/bulkRequest.hpp"
#include "uEarthscopeDataselectAmbassador/request.hpp"

using namespace UEarthscopeDataselectAmbassador;

namespace
{
// 2024-01-01T00:00:00 UTC
constexpr std::chrono::microseconds JAN_01_2024{std::chrono::seconds {1704067200}};
constexpr std::chrono::microseconds TEN_MINUTES{std::chrono::minutes {10}};

const std::string ANMO{"IU ANMO 00 BHZ 2024-01-01T00:00:00 2024-01-01T00:10:00"};
const std::string CTU{"UU CTU 01 HHZ 2024-01-01T00:00:00 2024-01-01T00:10:00"};

const std::string CANONICAL_ANMO{
    "IU ANMO 00 BHZ 2024-01-01T00:00:00.000000 2024-01-01T00:10:00.000000\n"};
const std::string CANONICAL_CTU{
    "UU CTU 01 HHZ 2024-01-01T00:00:00.000000 2024-01-01T00:10:00.000000\n"};
const std::string DEFAULT_OPTIONS{"format=miniseed\nnodata=204\n"};
}

TEST_CASE("UEarthscopeDataselectAmbassador::BulkRequest - parse",
          "[bulkRequest]")
{
    SECTION("FetchData body: retired options then selections")
    {
        const BulkRequest bulk{
            "quality=B\n"
            "minimumlength=0.0\n"
            "longestonly=true\n"
            + ANMO + "\n"
            + CTU + "\n"};
        REQUIRE(bulk.getRequests().size() == 2);
        REQUIRE(bulk.getDataFormat() == Request::DataFormat::MiniSEED);
        REQUIRE_FALSE(bulk.noDataIs404());
        REQUIRE(bulk.toString() == DEFAULT_OPTIONS + CANONICAL_ANMO + CANONICAL_CTU);
    }

    SECTION("Each selection is a full request")
    {
        const BulkRequest bulk{ANMO + "\n"};
        const auto &request = bulk.getRequests().at(0);
        REQUIRE(request.getNetwork() == "IU");
        REQUIRE(request.getStation() == "ANMO");
        REQUIRE(request.getLocationCode() == "00");
        REQUIRE(request.getChannel() == "BHZ");
        REQUIRE(request.getStartAndEndTime()
             == std::pair {JAN_01_2024, JAN_01_2024 + TEN_MINUTES});
    }

    SECTION("No trailing newline")
    {
        const BulkRequest bulk{ANMO};
        REQUIRE(bulk.getRequests().size() == 1);
    }

    SECTION("CRLF, blank lines, tabs, and extra spaces")
    {
        const BulkRequest bulk{
            "\r\n"
            "  IU\tANMO   00  BHZ 2024-01-01T00:00:00 2024-01-01T00:10:00  \r\n"
            "\n"
            "\t\n"
            + CTU + "\r\n"};
        REQUIRE(bulk.toString() == DEFAULT_OPTIONS + CANONICAL_ANMO + CANONICAL_CTU);
    }

    SECTION("Codes normalized, wildcards, and blank location")
    {
        const BulkRequest bulk{
            "uu ctu -- hh? 2024-01-01T00:00:00 2024-01-01T00:10:00\n"
            "UU * 01 HH* 2024-01-01T00:00:00 2024-01-01T00:10:00\n"};
        REQUIRE(bulk.toString()
             == DEFAULT_OPTIONS
              + "UU CTU -- HH? 2024-01-01T00:00:00.000000 2024-01-01T00:10:00.000000\n"
                "UU * 01 HH* 2024-01-01T00:00:00.000000 2024-01-01T00:10:00.000000\n");
    }

    SECTION("Fractional seconds and trailing Z")
    {
        const BulkRequest bulk{
            "IU ANMO 00 BHZ 2024-01-01T00:00:00.5Z 2024-01-01T00:10:00.123456Z\n"};
        REQUIRE(bulk.getRequests().at(0).getStartAndEndTime()
             == std::pair {JAN_01_2024 + std::chrono::milliseconds {500},
                           JAN_01_2024 + TEN_MINUTES
                         + std::chrono::microseconds {123456}});
    }

    SECTION("format and nodata options apply to every selection")
    {
        const BulkRequest bulk{
            "format=geocsv\n"
            "nodata=404\n"
            + ANMO + "\n"
            + CTU + "\n"};
        REQUIRE(bulk.getDataFormat() == Request::DataFormat::GeoCSV);
        REQUIRE(bulk.noDataIs404());
        for (const auto &request : bulk.getRequests())
        {
            REQUIRE(request.getDataFormat() == Request::DataFormat::GeoCSV);
            REQUIRE(request.noDataIs404());
        }
        REQUIRE(bulk.toString()
             == "format=geocsv\nnodata=404\n" + CANONICAL_ANMO + CANONICAL_CTU);
    }

    SECTION("Option keys are case-insensitive and may have spaces")
    {
        const BulkRequest bulk{"NoData = 404\nQuality=M\n" + ANMO + "\n"};
        REQUIRE(bulk.noDataIs404());
    }

    SECTION("1000 selections is the limit")
    {
        std::string body;
        for (int i = 0; i < 1000; ++i){body.append(ANMO + "\n");}
        REQUIRE(BulkRequest{body}.getRequests().size() == 1000);
    }

    SECTION("Copy and move")
    {
        const BulkRequest bulk{"nodata=404\n" + ANMO + "\n"};
        const BulkRequest copy{bulk}; // NOLINT(performance-unnecessary-copy-initialization)
        REQUIRE(copy.toString() == bulk.toString());
        BulkRequest source{"nodata=404\n" + ANMO + "\n"};
        const BulkRequest moved{std::move(source)};
        REQUIRE(moved.toString() == bulk.toString());
    }
}

TEST_CASE("UEarthscopeDataselectAmbassador::BulkRequest - rejects",
          "[bulkRequest]")
{
    SECTION("Empty, blank, or options-only bodies")
    {
        auto body = GENERATE(as<std::string> {},
                             "", "\n\n", "  \r\n\t\n",
                             "format=miniseed\nnodata=204\n");
        REQUIRE_THROWS_AS(BulkRequest{body}, std::invalid_argument);
    }

    SECTION("Selection lines need exactly six fields")
    {
        auto line = GENERATE(as<std::string> {},
            "IU ANMO 00 BHZ 2024-01-01T00:00:00",
            "IU ANMO BHZ 2024-01-01T00:00:00 2024-01-01T00:10:00",
            "IU ANMO 00 BHZ 2024-01-01T00:00:00 2024-01-01T00:10:00 extra");
        REQUIRE_THROWS_AS(BulkRequest{line + "\n"}, std::invalid_argument);
    }

    SECTION("Bad selections")
    {
        auto line = GENERATE(as<std::string> {},
            // end not after start
            "IU ANMO 00 BHZ 2024-01-01T00:10:00 2024-01-01T00:00:00",
            "IU ANMO 00 BHZ 2024-01-01T00:00:00 2024-01-01T00:00:00",
            // bad times
            "IU ANMO 00 BHZ 2024-13-01T00:00:00 2024-01-01T00:10:00",
            "IU ANMO 00 BHZ yesterday 2024-01-01T00:10:00",
            // bad codes
            "IU AN!MO 00 BHZ 2024-01-01T00:00:00 2024-01-01T00:10:00",
            "IUXX ANMO 00 BHZ 2024-01-01T00:00:00 2024-01-01T00:10:00",
            "IU ANMOXX 00 BHZ 2024-01-01T00:00:00 2024-01-01T00:10:00",
            "IU ANMO,CCM 00 BHZ 2024-01-01T00:00:00 2024-01-01T00:10:00",
            // query smuggling
            "IU AN&MO 00 BHZ 2024-01-01T00:00:00 2024-01-01T00:10:00",
            "IU AN%4DO 00 BHZ 2024-01-01T00:00:00 2024-01-01T00:10:00",
            "IU ANMO 00 BHZ 2024-01-01T00:00:00&nodata 2024-01-01T00:10:00");
        REQUIRE_THROWS_AS(BulkRequest{line + "\n"}, std::invalid_argument);
    }

    SECTION("One bad line rejects the whole body")
    {
        REQUIRE_THROWS_AS(
            BulkRequest{ANMO + "\nIU ANMO 00 BHZ nope 2024-01-01T00:10:00\n" + CTU},
            std::invalid_argument);
    }

    SECTION("Bad options")
    {
        auto options = GENERATE(as<std::string> {},
                                "format=sac.zip\n",
                                "format=\n",
                                "nodata=500\n",
                                "nodata=204\nnodata=404\n",
                                "format=miniseed\nformat=geocsv\n",
                                "net=IU\n",
                                "starttime=2024-01-01\n",
                                "unknown=1\n");
        REQUIRE_THROWS_AS(BulkRequest{options + ANMO + "\n"},
                          std::invalid_argument);
    }

    SECTION("More than 1000 selections")
    {
        std::string body;
        for (int i = 0; i < 1001; ++i){body.append(ANMO + "\n");}
        REQUIRE_THROWS_AS(BulkRequest{body}, std::invalid_argument);
    }

    SECTION("Error messages don't echo input")
    {
        const std::string marker{"QQZZ"};
        auto body = GENERATE_COPY(
            as<std::string> {},
            "IU " + marker + "!! 00 BHZ 2024-01-01T00:00:00 2024-01-01T00:10:00\n",
            "IU ANMO 00 BHZ " + marker + " 2024-01-01T00:10:00\n",
            "format=" + marker + "\n" + ANMO + "\n",
            marker + "=1\n" + ANMO + "\n",
            "IU ANMO 00 BHZ 2024-01-01T00:00:00 2024-01-01T00:10:00 " + marker + "\n");
        try
        {
            const BulkRequest bulk{body};
            FAIL("Expected std::invalid_argument");
        }
        catch (const std::invalid_argument &e)
        {
            REQUIRE(std::string {e.what()}.find(marker) == std::string::npos);
        }
    }
}
