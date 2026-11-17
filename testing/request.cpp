#include <chrono>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include "uEarthscopeDataselectAmbassador/request.hpp"

using namespace UEarthscopeDataselectAmbassador;

namespace
{
// 2024-01-01T00:00:00 UTC
constexpr std::chrono::microseconds JAN_01_2024{std::chrono::seconds {1704067200}};
constexpr std::chrono::microseconds TEN_MINUTES{std::chrono::minutes {10}};

const std::string EXPECTED_TIMES{
    "start=2024-01-01T00:00:00.000000&end=2024-01-01T00:10:00.000000"};
}

TEST_CASE("UEarthscopeDataselectAmbassador::Request - defaults", "[request]")
{
    const Request request;
    REQUIRE_FALSE(request.hasStartAndEndTime());
    REQUIRE_FALSE(request.getNetwork());
    REQUIRE_FALSE(request.getStation());
    REQUIRE_FALSE(request.getChannel());
    REQUIRE_FALSE(request.getLocationCode());
    REQUIRE(request.getDataFormat() == Request::DataFormat::MiniSEED);
    REQUIRE_FALSE(request.noDataIs404());
    REQUIRE_THROWS_AS(request.getStartAndEndTime(), std::runtime_error);
    REQUIRE_THROWS_AS(request.toString(), std::runtime_error);
}

TEST_CASE("UEarthscopeDataselectAmbassador::Request - times", "[request]")
{
    Request request;

    SECTION("Valid")
    {
        request.setStartAndEndTime(std::pair {JAN_01_2024,
                                              JAN_01_2024 + TEN_MINUTES});
        REQUIRE(request.hasStartAndEndTime());
        REQUIRE(request.getStartAndEndTime()
             == std::pair {JAN_01_2024, JAN_01_2024 + TEN_MINUTES});
    }

    SECTION("End must follow start")
    {
        REQUIRE_THROWS_AS(
            request.setStartAndEndTime(std::pair {JAN_01_2024, JAN_01_2024}),
            std::invalid_argument);
        REQUIRE_THROWS_AS(
            request.setStartAndEndTime(std::pair {JAN_01_2024 + TEN_MINUTES,
                                                  JAN_01_2024}),
            std::invalid_argument);
        REQUIRE_FALSE(request.hasStartAndEndTime());
    }

    SECTION("Sub-second precision reaches the query")
    {
        request.setStartAndEndTime(
            std::pair {JAN_01_2024 + std::chrono::microseconds {123456},
                       JAN_01_2024 + std::chrono::microseconds {1}
                                   + TEN_MINUTES});
        REQUIRE(request.toString().starts_with(
            "start=2024-01-01T00:00:00.123456"
            "&end=2024-01-01T00:10:00.000001&"));
    }

    SECTION("Pre-epoch times")
    {
        request.setStartAndEndTime(
            std::pair {std::chrono::microseconds {-1500000},
                       std::chrono::microseconds {0}});
        REQUIRE(request.toString().starts_with(
            "start=1969-12-31T23:59:58.500000"
            "&end=1970-01-01T00:00:00.000000&"));
    }
}

TEST_CASE("UEarthscopeDataselectAmbassador::Request - codes", "[request]")
{
    Request request;

    SECTION("Normalized to upper case")
    {
        request.setNetwork("uu");
        request.setStation("cwu");
        request.setChannel("hhz");
        request.setLocationCode("01");
        REQUIRE(request.getNetwork() == "UU");
        REQUIRE(request.getStation() == "CWU");
        REQUIRE(request.getChannel() == "HHZ");
        REQUIRE(request.getLocationCode() == "01");
    }

    SECTION("Wildcards allowed")
    {
        request.setNetwork("U*");
        request.setStation("*");
        request.setChannel("HH?");
        request.setLocationCode("0?");
        REQUIRE(request.getNetwork() == "U*");
        REQUIRE(request.getStation() == "*");
        REQUIRE(request.getChannel() == "HH?");
        REQUIRE(request.getLocationCode() == "0?");
    }

    SECTION("Maximum lengths")
    {
        request.setNetwork("UU");
        request.setStation("ABCDE");
        request.setChannel("HHZ");
        request.setLocationCode("01");
        REQUIRE_THROWS_AS(request.setNetwork("UUU"), std::invalid_argument);
        REQUIRE_THROWS_AS(request.setStation("ABCDEF"), std::invalid_argument);
        REQUIRE_THROWS_AS(request.setChannel("HHZZ"), std::invalid_argument);
        REQUIRE_THROWS_AS(request.setLocationCode("001"),
                          std::invalid_argument);
        // Failed sets leave the old value
        REQUIRE(request.getNetwork() == "UU");
        REQUIRE(request.getStation() == "ABCDE");
        REQUIRE(request.getChannel() == "HHZ");
        REQUIRE(request.getLocationCode() == "01");
    }

    SECTION("Blank location becomes --")
    {
        auto location = GENERATE(as<std::string> {}, "", " ", "  ", "--");
        request.setLocationCode(location);
        REQUIRE(request.getLocationCode() == "--");
    }

    SECTION("Empty network, station, channel are ignored")
    {
        request.setNetwork("");
        request.setStation("");
        request.setChannel("");
        REQUIRE_FALSE(request.getNetwork());
        REQUIRE_FALSE(request.getStation());
        REQUIRE_FALSE(request.getChannel());
    }

    SECTION("Illegal characters rejected")
    {
        // Injection attempts must never reach the upstream URL
        auto bad = GENERATE(as<std::string> {},
                            "U&", "U=", "U#", "U%", "U/", "U ", "U-",
                            "U,", "U.", "U\n", "U\r", "\xC3\xA9");
        REQUIRE_THROWS_AS(request.setNetwork(bad), std::invalid_argument);
        REQUIRE_FALSE(request.getNetwork());
    }

    SECTION("Comma lists rejected - one code per request")
    {
        REQUIRE_THROWS_AS(request.setChannel("HHZ,HHN"),
                          std::invalid_argument);
        REQUIRE_THROWS_AS(request.setStation("CWU,ARUT"),
                          std::invalid_argument);
    }

    SECTION("Lone or triple dash location rejected")
    {
        REQUIRE_THROWS_AS(request.setLocationCode("-"),
                          std::invalid_argument);
        REQUIRE_THROWS_AS(request.setLocationCode("---"),
                          std::invalid_argument);
    }

    SECTION("Error messages don't echo input")
    {
        const std::string marker{"<SCRIPT>"};
        try
        {
            request.setStation(marker);
            FAIL("Expected an exception");
        }
        catch (const std::invalid_argument &e)
        {
            REQUIRE(std::string {e.what()}.find("SCRIPT")
                 == std::string::npos);
        }
    }
}

TEST_CASE("UEarthscopeDataselectAmbassador::Request - toString", "[request]")
{
    Request request;
    request.setStartAndEndTime(std::pair {JAN_01_2024,
                                          JAN_01_2024 + TEN_MINUTES});

    SECTION("Times only")
    {
        REQUIRE(request.toString()
             == EXPECTED_TIMES + "&format=miniseed&nodata=204");
    }

    SECTION("Single station")
    {
        request.setNetwork("IU");
        request.setStation("ANMO");
        request.setChannel("BHZ");
        request.setLocationCode("00");
        REQUIRE(request.toString()
             == EXPECTED_TIMES
              + "&net=IU&sta=ANMO&cha=BHZ&loc=00&format=miniseed&nodata=204");
    }

    SECTION("Blank location")
    {
        request.setNetwork("UU");
        request.setStation("CWU");
        request.setChannel("HHZ");
        request.setLocationCode("");
        REQUIRE(request.toString()
             == EXPECTED_TIMES
              + "&net=UU&sta=CWU&cha=HHZ&loc=--&format=miniseed&nodata=204");
    }

    SECTION("nodata=404")
    {
        request.enableNoDataIs404();
        REQUIRE(request.noDataIs404());
        REQUIRE(request.toString().ends_with("&nodata=404"));
        request.disableNoDataIs404();
        REQUIRE(request.toString().ends_with("&nodata=204"));
    }

    SECTION("Formats")
    {
        auto [format, expected] = GENERATE(
            std::pair {Request::DataFormat::MiniSEED, "format=miniseed"},
            std::pair {Request::DataFormat::GeoCSV, "format=geocsv"},
            std::pair {Request::DataFormat::GeoCSV_INLINE,
                       "format=geocsv.inline"},
            std::pair {Request::DataFormat::GeoCSV_ZIP, "format=geocsv.zip"},
            std::pair {Request::DataFormat::GeoCSV_TSPAIR,
                       "format=geocsv.tspair"},
            std::pair {Request::DataFormat::GeoCSV_SLIST,
                       "format=geocsv.slist"});
        request.setDataFormat(format);
        REQUIRE(request.getDataFormat() == format);
        REQUIRE(request.toString()
             == EXPECTED_TIMES + "&" + expected + "&nodata=204");
    }
}

TEST_CASE("UEarthscopeDataselectAmbassador::Request - copy and move",
          "[request]")
{
    Request request;
    request.setStartAndEndTime(std::pair {JAN_01_2024,
                                          JAN_01_2024 + TEN_MINUTES});
    request.setNetwork("UU");
    request.setStation("CWU");
    request.enableNoDataIs404();
    const auto expected = request.toString();

    Request copy{request};
    REQUIRE(copy.toString() == expected);

    copy.setStation("ARUT");
    REQUIRE(request.getStation() == "CWU");

    Request assigned;
    assigned = request;
    REQUIRE(assigned.toString() == expected);

    const Request moved{std::move(assigned)};
    REQUIRE(moved.toString() == expected);

    Request moveAssigned;
    moveAssigned = std::move(copy);
    REQUIRE(moveAssigned.getStation() == "ARUT");
}

TEST_CASE("UEarthscopeDataselectAmbassador::Request - parse query",
          "[request][parser]")
{
    const std::string times{
        "start=2024-01-01T00:00:00&end=2024-01-01T00:10:00"};

    SECTION("Single station")
    {
        const Request request{
            "net=UU&sta=CWU&loc=01&cha=HHZ&" + times};
        REQUIRE(request.toString()
             == EXPECTED_TIMES
              + "&net=UU&sta=CWU&cha=HHZ&loc=01&format=miniseed&nodata=204");
    }

    SECTION("Long names, mixed-case keys and values, any order")
    {
        const Request request{
            "endtime=2024-01-01T00:10:00&CHANNEL=hhz&Location=01"
            "&station=cwu&Network=uu&StartTime=2024-01-01T00:00:00"};
        REQUIRE(request.toString()
             == EXPECTED_TIMES
              + "&net=UU&sta=CWU&cha=HHZ&loc=01&format=miniseed&nodata=204");
    }

    SECTION("Percent-encoding decoded")
    {
        const Request request{
            "net=UU&sta=CWU&cha=HH%3F"
            "&start=2024-01-01T00%3A00%3A00&end=2024-01-01T00%3a10%3a00"};
        REQUIRE(request.getChannel() == "HH?");
        REQUIRE(request.getStartAndEndTime()
             == std::pair {JAN_01_2024, JAN_01_2024 + TEN_MINUTES});
    }

    SECTION("Plus as the date/time separator")
    {
        const Request request{
            "start=2024-01-01+00:00:00&end=2024-01-01+00:10:00"};
        REQUIRE(request.getStartAndEndTime()
             == std::pair {JAN_01_2024, JAN_01_2024 + TEN_MINUTES});
    }

    SECTION("Bare dates")
    {
        const Request request{"start=2024-01-01&end=2024-01-02"};
        REQUIRE(request.getStartAndEndTime()
             == std::pair {JAN_01_2024,
                           JAN_01_2024 + std::chrono::hours {24}});
    }

    SECTION("Fractional seconds and trailing Z")
    {
        const Request request{
            "start=2024-01-01T00:00:00.5Z&end=2024-01-01T00:10:00.123456Z"};
        REQUIRE(request.getStartAndEndTime()
             == std::pair {JAN_01_2024 + std::chrono::milliseconds {500},
                           JAN_01_2024 + TEN_MINUTES
                         + std::chrono::microseconds {123456}});
    }

    SECTION("Blank location spellings")
    {
        auto location = GENERATE(as<std::string> {},
                                 "loc=--", "loc=", "loc", "loc=%20%20",
                                 "loc=++", "location=--");
        const Request request{location + "&" + times};
        REQUIRE(request.getLocationCode() == "--");
    }

    SECTION("nodata")
    {
        REQUIRE(Request{"nodata=404&" + times}.noDataIs404());
        REQUIRE_FALSE(Request{"nodata=204&" + times}.noDataIs404());
        REQUIRE_FALSE(Request{times}.noDataIs404());
    }

    SECTION("Formats")
    {
        auto [name, format] = GENERATE(
            std::pair {"miniseed", Request::DataFormat::MiniSEED},
            std::pair {"geocsv", Request::DataFormat::GeoCSV},
            std::pair {"geocsv.inline", Request::DataFormat::GeoCSV_INLINE},
            std::pair {"geocsv.zip", Request::DataFormat::GeoCSV_ZIP},
            std::pair {"geocsv.tspair", Request::DataFormat::GeoCSV_TSPAIR},
            std::pair {"geocsv.slist", Request::DataFormat::GeoCSV_SLIST});
        const Request request{"format=" + std::string {name} + "&" + times};
        REQUIRE(request.getDataFormat() == format);
    }

    SECTION("Retired parameters dropped")
    {
        const Request request{
            "net=UU&quality=B&minimumlength=0.0&LongestOnly=true&" + times};
        REQUIRE(request.toString()
             == EXPECTED_TIMES + "&net=UU&format=miniseed&nodata=204");
    }
}

TEST_CASE("UEarthscopeDataselectAmbassador::Request - parse query rejects",
          "[request][parser]")
{
    const std::string times{
        "start=2024-01-01T00:00:00&end=2024-01-01T00:10:00"};

    SECTION("Missing times")
    {
        auto query = GENERATE(as<std::string> {},
            "",
            "net=UU&sta=CWU",
            "start=2024-01-01T00:00:00",
            "end=2024-01-01T00:10:00");
        REQUIRE_THROWS_AS(Request {query}, std::invalid_argument);
    }

    SECTION("Bad parameters")
    {
        auto query = GENERATE(as<std::string> {},
            "bogus=1",
            "sta=CWU&station=ARUT",           // duplicate via alias
            "net=UU&net=UU",                  // duplicate, same value
            "sta=",                           // would wildcard every station
            "sta",
            "net=",
            "cha=",
            "start=",
            "format=",
            "format=sac.zip",
            "format=MSEED",
            "nodata=200",
            "nodata",
            "sta=CWU%26net%3DXX",             // decoded '&' must not smuggle a parameter
            "sta=CWU%ZZ",                     // malformed percent-encoding
            "net=UUU",
            "cha=HHZ,HHN");
        REQUIRE_THROWS_AS(Request {query + "&" + times},
                          std::invalid_argument);
    }

    SECTION("End not after start")
    {
        REQUIRE_THROWS_AS(
            Request {"start=2024-01-01T00:10:00&end=2024-01-01T00:00:00"},
            std::invalid_argument);
        REQUIRE_THROWS_AS(
            Request {"start=2024-01-01&end=2024-01-01"},
            std::invalid_argument);
    }

    SECTION("Error messages don't echo values")
    {
        auto query = GENERATE(as<std::string> {},
            "sta=%3CSCRIPT%3E", "nodata=%3CSCRIPT%3E",
            "format=%3CSCRIPT%3E", "start=%3CSCRIPT%3E");
        try
        {
            const Request request{query + "&" + times};
            FAIL("Expected an exception");
        }
        catch (const std::invalid_argument &e)
        {
            REQUIRE(std::string {e.what()}.find("SCRIPT")
                 == std::string::npos);
        }
    }
}

// Strict on purpose: a typo like T25:00 must be a 400, not a different
// window that returns 200 with the wrong data.
TEST_CASE("UEarthscopeDataselectAmbassador::Request - parse time rejects",
          "[request][parser][time]")
{
    auto start = GENERATE(as<std::string> {},
        "2023-02-29",
        "2024-13-01",
        "2024-00-10",
        "2024-01-00",
        "2024-01-01T24:00:00",
        "2024-01-01T00:60:00",
        "2024-01-01T00:00:60",
        "2024-01-01T00:00:00.",
        "2024-01-01T00:00:00.1234567",
        "2024-1-01",
        "2024-01-01T00:00",
        "2024-01-01X00:00:00",
        "2024-01-01T00:00:00junk",
        "+024-01-01",
        "2024-01-01T-1:00:00",
        "2024-001",
        "Z");
    REQUIRE_THROWS_AS(
        Request {"start=" + start + "&end=2025-01-01T00:00:00"},
        std::invalid_argument);
}
