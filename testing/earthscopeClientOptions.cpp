#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <random>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <catch2/catch_test_macros.hpp>
#include "uEarthscopeDataselectAmbassador/earthscopeClientOptions.hpp"

using namespace UEarthscopeDataselectAmbassador;

namespace
{

/// A scratch directory that is removed when the test case ends, pass or fail.
class TemporaryDirectory
{
public:
    TemporaryDirectory()
    {
        std::random_device device;
        std::uniform_int_distribution<std::uint64_t> distribution;
        mPath = std::filesystem::temp_directory_path()
              / ("uEarthscopeDataselectAmbassadorTest_"
                + std::to_string(distribution(device)));
        std::filesystem::create_directories(mPath);
    }
    TemporaryDirectory(const TemporaryDirectory &) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory &) = delete;
    ~TemporaryDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(mPath, error);
    }
    /// Writes contents to name inside the directory and returns its path.
    [[nodiscard]] std::filesystem::path write(const std::string &name,
                                              const std::string &contents) const
    {
        auto path = mPath / name;
        std::ofstream file(path);
        file << contents;
        return path;
    }
    [[nodiscard]] const std::filesystem::path &path() const noexcept
    {
        return mPath;
    }
private:
    std::filesystem::path mPath;
};

// Obviously fake credentials - never put real ones in a test
const std::string USER{"fake.user@example.org"};
const std::string PASSWORD{"notARealPassword"};
const std::string TOKEN{"fake.jwt.token"};

}

TEST_CASE("UEarthscopeDataselectAmbassador::EarthscopeClientOptions",
          "[options]")
{
    EarthscopeClientOptions options;

    SECTION("Defaults")
    {
        REQUIRE(options.getEndpoint()
             == "https://service.earthscope.org/fdsnws/dataselect/1");
        REQUIRE_FALSE(options.hasAccessToken());
        REQUIRE_FALSE(options.hasLegacyCredentials());
        REQUIRE(options.getRequestTimeOut() == std::chrono::seconds {15});
        REQUIRE(options.getConnectTimeOut() == std::chrono::seconds {5});
        REQUIRE(options.getMaximumResponseSizeInBytes()
             == int64_t {32}*1024*1024);
        REQUIRE_THROWS_AS(options.getAccessToken(), std::runtime_error);
        REQUIRE_THROWS_AS(options.getLegacyCredentials(), std::runtime_error);
    }

    SECTION("Access token")
    {
        options.setAccessToken(TOKEN);
        REQUIRE(options.hasAccessToken());
        REQUIRE(options.getAccessToken() == TOKEN);
        REQUIRE_THROWS_AS(options.setAccessToken(""), std::invalid_argument);
        // A rejected set leaves the previous value alone
        REQUIRE(options.getAccessToken() == TOKEN);
    }

    SECTION("Legacy credentials")
    {
        options.setLegacyCredentials(std::pair {USER, PASSWORD});
        REQUIRE(options.hasLegacyCredentials());
        auto [user, password] = options.getLegacyCredentials();
        REQUIRE(user == USER);
        REQUIRE(password == PASSWORD);
    }

    SECTION("Legacy credentials - surrounding whitespace trimmed from user")
    {
        options.setLegacyCredentials(std::pair {"  " + USER + "\n", PASSWORD});
        REQUIRE(options.getLegacyCredentials().first == USER);
    }

    SECTION("Legacy credentials - invalid")
    {
        REQUIRE_THROWS_AS(
            options.setLegacyCredentials(std::pair {std::string {}, PASSWORD}),
            std::invalid_argument);
        REQUIRE_THROWS_AS(
            options.setLegacyCredentials(std::pair {std::string {"   "},
                                                    PASSWORD}),
            std::invalid_argument);
        REQUIRE_THROWS_AS(
            options.setLegacyCredentials(std::pair {USER, std::string {}}),
            std::invalid_argument);
        REQUIRE_FALSE(options.hasLegacyCredentials());
    }

    SECTION("Endpoint")
    {
        options.setEndpoint("  http://localhost:8080/fdsnws/dataselect/1\n");
        REQUIRE(options.getEndpoint()
             == "http://localhost:8080/fdsnws/dataselect/1");
        REQUIRE_THROWS_AS(options.setEndpoint(""), std::invalid_argument);
        REQUIRE_THROWS_AS(options.setEndpoint(" \t "), std::invalid_argument);
    }

    SECTION("Timeouts")
    {
        // Sub-second values must survive; curl's _MS options need them
        options.setRequestTimeOut(std::chrono::milliseconds {750});
        options.setConnectTimeOut(std::chrono::milliseconds {250});
        REQUIRE(options.getRequestTimeOut() == std::chrono::milliseconds {750});
        REQUIRE(options.getConnectTimeOut() == std::chrono::milliseconds {250});
        REQUIRE_THROWS_AS(options.setRequestTimeOut(std::chrono::milliseconds {0}),
                          std::invalid_argument);
        REQUIRE_THROWS_AS(options.setConnectTimeOut(std::chrono::milliseconds {-1}),
                          std::invalid_argument);
    }

    SECTION("Maximum response size")
    {
        options.setMaximumResponseSizeInBytes(int64_t {64}*1024*1024);
        REQUIRE(options.getMaximumResponseSizeInBytes()
             == int64_t {64}*1024*1024);
        REQUIRE_THROWS_AS(options.setMaximumResponseSizeInBytes(0),
                          std::invalid_argument);
        REQUIRE_THROWS_AS(options.setMaximumResponseSizeInBytes(-1),
                          std::invalid_argument);
    }

    SECTION("Copy and move")
    {
        options.setAccessToken(TOKEN);
        options.setRequestTimeOut(std::chrono::seconds {3});

        EarthscopeClientOptions copy{options};
        REQUIRE(copy.getAccessToken() == TOKEN);
        REQUIRE(copy.getRequestTimeOut() == std::chrono::seconds {3});

        // The copy is deep
        copy.setRequestTimeOut(std::chrono::seconds {4});
        REQUIRE(options.getRequestTimeOut() == std::chrono::seconds {3});

        EarthscopeClientOptions assigned;
        assigned = copy;
        REQUIRE(assigned.getRequestTimeOut() == std::chrono::seconds {4});

        const EarthscopeClientOptions moved{std::move(copy)};
        REQUIRE(moved.getAccessToken() == TOKEN);
        REQUIRE(moved.getRequestTimeOut() == std::chrono::seconds {4});
    }
}

// Digest hashes the user name exactly as given, so case must survive.
// Characters that would break the Authorization header are dropped.
TEST_CASE("UEarthscopeDataselectAmbassador::EarthscopeClientOptions - "
          "user name sanitization", "[options]")
{
    EarthscopeClientOptions options;

    SECTION("Case preserved")
    {
        options.setLegacyCredentials(std::pair {std::string {"Fake.User"},
                                                PASSWORD});
        REQUIRE(options.getLegacyCredentials().first == "Fake.User");
    }

    SECTION("Characters outside the old allow-list survive")
    {
        options.setLegacyCredentials(
            std::pair {std::string {"fake+user@example.org"}, PASSWORD});
        REQUIRE(options.getLegacyCredentials().first
             == "fake+user@example.org");
    }

    SECTION("CR, LF, colon, and blanks are dropped")
    {
        options.setLegacyCredentials(
            std::pair {std::string {" \tFake:User \r\n"}, PASSWORD});
        REQUIRE(options.getLegacyCredentials().first == "FakeUser");
        options.setLegacyCredentials(
            std::pair {std::string {"fake user"}, PASSWORD});
        REQUIRE(options.getLegacyCredentials().first == "fakeuser");
    }

    SECTION("Nothing left is an error")
    {
        REQUIRE_THROWS_AS(
            options.setLegacyCredentials(
                std::pair {std::string {" :\r\n: "}, PASSWORD}),
            std::invalid_argument);
    }

    SECTION("Password is untouched")
    {
        options.setLegacyCredentials(
            std::pair {USER, std::string {"pass:Word With Spaces"}});
        REQUIRE(options.getLegacyCredentials().second
             == "pass:Word With Spaces");
    }
}

TEST_CASE("UEarthscopeDataselectAmbassador::EarthscopeClientOptions - "
          "initialization file", "[options][ini]")
{
    const TemporaryDirectory directory;

    SECTION("Missing file")
    {
        REQUIRE_THROWS_AS(
            EarthscopeClientOptions::fromInitializationFile(
                directory.path() / "doesNotExist.ini"),
            std::invalid_argument);
    }

    SECTION("Legacy credentials with defaults")
    {
        const auto ini = directory.write("test.ini",
            "[Earthscope]\n"
            "user = " + USER + "\n"
            "password = " + PASSWORD + "\n");
        const auto options
            = EarthscopeClientOptions::fromInitializationFile(ini);
        REQUIRE(options.hasLegacyCredentials());
        REQUIRE_FALSE(options.hasAccessToken());
        REQUIRE(options.getLegacyCredentials()
             == std::pair {USER, PASSWORD});
        REQUIRE(options.getEndpoint()
             == "https://service.earthscope.org/fdsnws/dataselect/1");
        REQUIRE(options.getRequestTimeOut() == std::chrono::seconds {15});
        REQUIRE(options.getConnectTimeOut() == std::chrono::seconds {5});
        REQUIRE(options.getMaximumResponseSizeInBytes()
             == int64_t {32}*1024*1024);
    }

    SECTION("All settings")
    {
        const auto ini = directory.write("test.ini",
            "[Earthscope]\n"
            "endpoint = http://localhost:8080/fdsnws/dataselect/1\n"
            "user = " + USER + "\n"
            "password = " + PASSWORD + "\n"
            "requestTimeOutInSeconds = 30\n"
            "connectTimeOutInSeconds = 2\n"
            "maximumResponseSizeInMegabytes = 64\n");
        const auto options
            = EarthscopeClientOptions::fromInitializationFile(ini);
        REQUIRE(options.getEndpoint()
             == "http://localhost:8080/fdsnws/dataselect/1");
        REQUIRE(options.getRequestTimeOut() == std::chrono::seconds {30});
        REQUIRE(options.getConnectTimeOut() == std::chrono::seconds {2});
        REQUIRE(options.getMaximumResponseSizeInBytes()
             == int64_t {64}*1024*1024);
    }

    SECTION("Token takes precedence over legacy credentials")
    {
        const auto ini = directory.write("test.ini",
            "[Earthscope]\n"
            "accessToken = " + TOKEN + "\n"
            "user = " + USER + "\n"
            "password = " + PASSWORD + "\n");
        const auto options
            = EarthscopeClientOptions::fromInitializationFile(ini);
        REQUIRE(options.hasAccessToken());
        REQUIRE(options.getAccessToken() == TOKEN);
        REQUIRE_FALSE(options.hasLegacyCredentials());
    }

    SECTION("Secrets from files are trimmed")
    {
        // Kubernetes Secrets made from a shell usually end in a newline
        const auto userFile = directory.write("user", USER + "\n");
        const auto passwordFile = directory.write("password",
                                                  PASSWORD + "\r\n");
        const auto ini = directory.write("test.ini",
            "[Earthscope]\n"
            "userFile = " + userFile.string() + "\n"
            "passwordFile = " + passwordFile.string() + "\n");
        const auto options
            = EarthscopeClientOptions::fromInitializationFile(ini);
        REQUIRE(options.getLegacyCredentials()
             == std::pair {USER, PASSWORD});
    }

    SECTION("Token from file")
    {
        const auto tokenFile = directory.write("token", TOKEN + "\n");
        const auto ini = directory.write("test.ini",
            "[Earthscope]\n"
            "accessTokenFile = " + tokenFile.string() + "\n");
        const auto options
            = EarthscopeClientOptions::fromInitializationFile(ini);
        REQUIRE(options.getAccessToken() == TOKEN);
    }

    SECTION("Inline and file both set is an error")
    {
        const auto passwordFile = directory.write("password", PASSWORD);
        const auto ini = directory.write("test.ini",
            "[Earthscope]\n"
            "user = " + USER + "\n"
            "password = " + PASSWORD + "\n"
            "passwordFile = " + passwordFile.string() + "\n");
        REQUIRE_THROWS_AS(
            EarthscopeClientOptions::fromInitializationFile(ini),
            std::invalid_argument);
    }

    SECTION("Secret file missing or empty")
    {
        const auto emptyFile = directory.write("password", " \n");
        auto ini = directory.write("test.ini",
            "[Earthscope]\n"
            "user = " + USER + "\n"
            "passwordFile = " + emptyFile.string() + "\n");
        REQUIRE_THROWS_AS(
            EarthscopeClientOptions::fromInitializationFile(ini),
            std::invalid_argument);

        ini = directory.write("test.ini",
            "[Earthscope]\n"
            "user = " + USER + "\n"
            "passwordFile = " + (directory.path() / "nope").string() + "\n");
        REQUIRE_THROWS_AS(
            EarthscopeClientOptions::fromInitializationFile(ini),
            std::invalid_argument);
    }

    SECTION("No credentials is an error")
    {
        const auto ini = directory.write("test.ini",
            "[Earthscope]\n"
            "endpoint = http://localhost:8080/fdsnws/dataselect/1\n");
        REQUIRE_THROWS_AS(
            EarthscopeClientOptions::fromInitializationFile(ini),
            std::invalid_argument);
    }

    SECTION("User without password is an error")
    {
        const auto ini = directory.write("test.ini",
            "[Earthscope]\n"
            "user = " + USER + "\n");
        REQUIRE_THROWS_AS(
            EarthscopeClientOptions::fromInitializationFile(ini),
            std::invalid_argument);
    }

    SECTION("Non-positive settings are rejected")
    {
        const auto ini = directory.write("test.ini",
            "[Earthscope]\n"
            "user = " + USER + "\n"
            "password = " + PASSWORD + "\n"
            "requestTimeOutInSeconds = 0\n");
        REQUIRE_THROWS_AS(
            EarthscopeClientOptions::fromInitializationFile(ini),
            std::invalid_argument);
    }

    SECTION("Custom section")
    {
        const auto ini = directory.write("test.ini",
            "[Earthscope]\n"
            "user = wrong.user\n"
            "password = wrongPassword\n"
            "[EarthscopeStaging]\n"
            "user = " + USER + "\n"
            "password = " + PASSWORD + "\n");
        const auto options
            = EarthscopeClientOptions::fromInitializationFile(
                ini, "EarthscopeStaging");
        REQUIRE(options.getLegacyCredentials()
             == std::pair {USER, PASSWORD});
    }
}
