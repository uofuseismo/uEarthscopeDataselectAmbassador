#include <chrono>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include "uEarthscopeDataselectAmbassador/grpc/serverOptions.hpp"
#include "../temporaryDirectory.hpp"

using namespace UEarthscopeDataselectAmbassador::GRPC;

namespace
{

/// @brief Writes an ini file with the given body under [Server] and loads it.
ServerOptions load(const Testing::TemporaryDirectory &directory,
                   const std::string &body,
                   const std::string &section = "Server")
{
    auto path = directory.write("server.ini",
                                "[" + section + "]\n" + body);
    return ServerOptions::fromInitializationFile(path, section);
}

/// @brief A PEM-looking value.  The options never parse it; they only
///        carry it, so anything non-empty will do.
const std::string certificate{"-----BEGIN CERTIFICATE-----\nMIIB\n-----END CERTIFICATE-----"};
const std::string key{"-----BEGIN PRIVATE KEY-----\nMIIE\n-----END PRIVATE KEY-----"};

}

TEST_CASE("UEarthscopeDataselectAmbassador::GRPC::ServerOptions defaults",
          "[grpc][serverOptions]")
{
    const ServerOptions options;
    REQUIRE(options.getHost() == "localhost");
    REQUIRE(options.getPort() == 50000);
    REQUIRE(options.getAddress() == "localhost:50000");
    REQUIRE_FALSE(options.getAccessToken().has_value());
    REQUIRE_FALSE(options.hasServerCertificate());
    REQUIRE_FALSE(options.hasServerKey());
    REQUIRE_THROWS_AS(options.getServerCertificate(), std::runtime_error);
    REQUIRE_THROWS_AS(options.getServerKey(), std::runtime_error);
    REQUIRE_FALSE(options.getClientCertificate().has_value());
    REQUIRE_FALSE(options.isReflectionEnabled());
    REQUIRE(options.getMaximumRequestMessageSizeInBytes() == 65536);
    REQUIRE(options.getMaximumConnectionAge() == std::chrono::minutes {3});
    REQUIRE(options.getMaximumConnectionAgeGracePeriod()
         == std::chrono::seconds {2});
    REQUIRE(options.getMaximumRequestIdentifierSizeInBytes() == 128);
}

TEST_CASE("UEarthscopeDataselectAmbassador::GRPC::ServerOptions setters",
          "[grpc][serverOptions]")
{
    ServerOptions options;

    SECTION("Host")
    {
        options.setHost("ambassador.example.edu");
        REQUIRE(options.getHost() == "ambassador.example.edu");
        // Whitespace from a hand-edited config is dropped
        options.setHost("  0.0.0.0\t\n");
        REQUIRE(options.getHost() == "0.0.0.0");
        REQUIRE_THROWS_AS(options.setHost(""), std::invalid_argument);
        REQUIRE_THROWS_AS(options.setHost(" \t "), std::invalid_argument);
        REQUIRE(options.getHost() == "0.0.0.0");
    }
    SECTION("Port and address")
    {
        options.setHost("0.0.0.0");
        options.setPort(50051);
        REQUIRE(options.getPort() == 50051);
        REQUIRE(options.getAddress() == "0.0.0.0:50051");
        options.setPort(1);
        REQUIRE(options.getPort() == 1);
        options.setPort(65535);
        REQUIRE(options.getPort() == 65535);
        REQUIRE_THROWS_AS(options.setPort(0), std::invalid_argument);
        REQUIRE(options.getPort() == 65535);
    }
    SECTION("Server certificate and key")
    {
        options.setServerCertificate(certificate);
        REQUIRE(options.hasServerCertificate());
        REQUIRE(options.getServerCertificate() == certificate);
        options.setServerKey(key);
        REQUIRE(options.hasServerKey());
        REQUIRE(options.getServerKey() == key);
        REQUIRE_THROWS_AS(options.setServerCertificate(""),
                          std::invalid_argument);
        REQUIRE_THROWS_AS(options.setServerKey(""), std::invalid_argument);
        REQUIRE(options.getServerCertificate() == certificate);
        REQUIRE(options.getServerKey() == key);
    }
    SECTION("Client certificate")
    {
        options.setClientCertificate(certificate);
        REQUIRE(options.getClientCertificate() == certificate);
        REQUIRE_THROWS_AS(options.setClientCertificate(""),
                          std::invalid_argument);
    }
    SECTION("Access token")
    {
        options.setAccessToken("not-a-real-token");
        REQUIRE(options.getAccessToken() == "not-a-real-token");
        REQUIRE_THROWS_AS(options.setAccessToken(""), std::invalid_argument);
        REQUIRE(options.getAccessToken() == "not-a-real-token");
    }
    SECTION("Reflection")
    {
        options.enableReflection();
        REQUIRE(options.isReflectionEnabled());
        options.disableReflection();
        REQUIRE_FALSE(options.isReflectionEnabled());
    }
    SECTION("Maximum request message size")
    {
        options.setMaximumRequestMessageSizeInBytes(1);
        REQUIRE(options.getMaximumRequestMessageSizeInBytes() == 1);
        options.setMaximumRequestMessageSizeInBytes(4*1024*1024);
        REQUIRE(options.getMaximumRequestMessageSizeInBytes() == 4*1024*1024);
        REQUIRE_THROWS_AS(options.setMaximumRequestMessageSizeInBytes(0),
                          std::invalid_argument);
        REQUIRE_THROWS_AS(options.setMaximumRequestMessageSizeInBytes(-1),
                          std::invalid_argument);
    }
    SECTION("Maximum connection age")
    {
        options.setMaximumConnectionAge(std::chrono::milliseconds {1});
        REQUIRE(options.getMaximumConnectionAge()
             == std::chrono::milliseconds {1});
        REQUIRE_THROWS_AS(
            options.setMaximumConnectionAge(std::chrono::milliseconds {0}),
            std::invalid_argument);
        REQUIRE_THROWS_AS(
            options.setMaximumConnectionAge(std::chrono::milliseconds {-5}),
            std::invalid_argument);
    }
    SECTION("Grace period may be zero but not negative")
    {
        options.setMaximumConnectionAgeGracePeriod(
            std::chrono::milliseconds {0});
        REQUIRE(options.getMaximumConnectionAgeGracePeriod()
             == std::chrono::milliseconds {0});
        REQUIRE_THROWS_AS(
            options.setMaximumConnectionAgeGracePeriod(
                std::chrono::milliseconds {-1}),
            std::invalid_argument);
    }
    SECTION("Maximum request identifier size is in [1, 8192]")
    {
        options.setMaximumRequestIdentifierSizeInBytes(1);
        REQUIRE(options.getMaximumRequestIdentifierSizeInBytes() == 1);
        options.setMaximumRequestIdentifierSizeInBytes(8192);
        REQUIRE(options.getMaximumRequestIdentifierSizeInBytes() == 8192);
        REQUIRE_THROWS_AS(options.setMaximumRequestIdentifierSizeInBytes(0),
                          std::invalid_argument);
        REQUIRE_THROWS_AS(options.setMaximumRequestIdentifierSizeInBytes(8193),
                          std::invalid_argument);
        REQUIRE(options.getMaximumRequestIdentifierSizeInBytes() == 8192);
    }
}

TEST_CASE("UEarthscopeDataselectAmbassador::GRPC::ServerOptions copy and move",
          "[grpc][serverOptions]")
{
    ServerOptions options;
    options.setHost("0.0.0.0");
    options.setPort(50051);
    options.setServerCertificate(certificate);
    options.setServerKey(key);
    options.setClientCertificate(certificate);
    options.setAccessToken("not-a-real-token");
    options.enableReflection();
    options.setMaximumRequestMessageSizeInBytes(1024);
    options.setMaximumConnectionAge(std::chrono::seconds {30});
    options.setMaximumConnectionAgeGracePeriod(std::chrono::milliseconds {500});
    options.setMaximumRequestIdentifierSizeInBytes(64);

    auto check = [](const ServerOptions &copy)
    {
        REQUIRE(copy.getAddress() == "0.0.0.0:50051");
        REQUIRE(copy.getServerCertificate() == certificate);
        REQUIRE(copy.getServerKey() == key);
        REQUIRE(copy.getClientCertificate() == certificate);
        REQUIRE(copy.getAccessToken() == "not-a-real-token");
        REQUIRE(copy.isReflectionEnabled());
        REQUIRE(copy.getMaximumRequestMessageSizeInBytes() == 1024);
        REQUIRE(copy.getMaximumConnectionAge() == std::chrono::seconds {30});
        REQUIRE(copy.getMaximumConnectionAgeGracePeriod()
             == std::chrono::milliseconds {500});
        REQUIRE(copy.getMaximumRequestIdentifierSizeInBytes() == 64);
    };

    SECTION("Copy constructor")
    {
        // The copy itself is what is under test
        const ServerOptions copy{options}; // NOLINT(performance-unnecessary-copy-initialization)
        check(copy);
    }
    SECTION("Copy assignment is deep")
    {
        ServerOptions copy;
        copy = options;
        check(copy);
        options.setPort(1);
        REQUIRE(copy.getPort() == 50051);
    }
    SECTION("Move constructor")
    {
        const ServerOptions moved{std::move(options)};
        check(moved);
    }
    SECTION("Move assignment")
    {
        ServerOptions moved;
        // Catch reruns the case per SECTION, so options is fresh here
        moved = std::move(options); // NOLINT(bugprone-use-after-move)
        check(moved);
    }
}

TEST_CASE("UEarthscopeDataselectAmbassador::GRPC::ServerOptions from ini",
          "[grpc][serverOptions]")
{
    const Testing::TemporaryDirectory directory;

    SECTION("Missing file")
    {
        REQUIRE_THROWS_AS(
            ServerOptions::fromInitializationFile(
                directory.path() / "doesNotExist.ini"),
            std::invalid_argument);
    }
    SECTION("Empty section gives the defaults")
    {
        const auto options = load(directory, "");
        const ServerOptions defaults;
        REQUIRE(options.getAddress() == defaults.getAddress());
        REQUIRE_FALSE(options.hasServerCertificate());
        REQUIRE_FALSE(options.hasServerKey());
        REQUIRE_FALSE(options.getAccessToken().has_value());
        REQUIRE_FALSE(options.isReflectionEnabled());
        REQUIRE(options.getMaximumRequestMessageSizeInBytes()
             == defaults.getMaximumRequestMessageSizeInBytes());
        REQUIRE(options.getMaximumConnectionAge()
             == defaults.getMaximumConnectionAge());
        REQUIRE(options.getMaximumConnectionAgeGracePeriod()
             == defaults.getMaximumConnectionAgeGracePeriod());
        REQUIRE(options.getMaximumRequestIdentifierSizeInBytes()
             == defaults.getMaximumRequestIdentifierSizeInBytes());
    }
    SECTION("Every plain setting is read")
    {
        const auto options = load(directory,
            "host = 0.0.0.0\n"
            "port = 50051\n"
            "enableReflection = true\n"
            "maximumRequestMessageSizeInBytes = 2048\n"
            "maximumConnectionAgeInMilliSeconds = 60000\n"
            "maximumConnectionAgeGracePeriodInMilliSeconds = 0\n"
            "maximumRequestIdentifierSizeInBytes = 64\n");
        REQUIRE(options.getAddress() == "0.0.0.0:50051");
        REQUIRE(options.isReflectionEnabled());
        REQUIRE(options.getMaximumRequestMessageSizeInBytes() == 2048);
        REQUIRE(options.getMaximumRequestIdentifierSizeInBytes() == 64);
        REQUIRE(options.getMaximumConnectionAge() == std::chrono::minutes {1});
        REQUIRE(options.getMaximumConnectionAgeGracePeriod()
             == std::chrono::milliseconds {0});
    }
    SECTION("A different section name")
    {
        const auto options = load(directory, "port = 50052\n", "GRPCServer");
        REQUIRE(options.getPort() == 50052);
    }
    SECTION("Port out of range or garbage")
    {
        for (const auto *port : {"0", "-1", "65536", "fifty"})
        {
            CAPTURE(port);
            REQUIRE_THROWS_AS(load(directory, std::string{"port = "} + port + "\n"),
                              std::invalid_argument);
        }
    }
    SECTION("Empty host")
    {
        REQUIRE_THROWS_AS(load(directory, "host =\n"), std::invalid_argument);
    }
    SECTION("Garbage and out-of-range values are rejected, not defaulted")
    {
        for (const auto *line : {"enableReflection = maybe\n",
                                 "maximumRequestMessageSizeInBytes = big\n",
                                 "maximumRequestMessageSizeInBytes = 0\n",
                                 "maximumConnectionAgeInMilliSeconds = 0\n",
                                 "maximumConnectionAgeGracePeriodInMilliSeconds = -1\n",
                                 "maximumRequestIdentifierSizeInBytes = 0\n",
                                 "maximumRequestIdentifierSizeInBytes = 8193\n",
                                 "maximumRequestIdentifierSizeInBytes = long\n"})
        {
            CAPTURE(line);
            REQUIRE_THROWS_AS(load(directory, line), std::invalid_argument);
        }
    }
    SECTION("Server key and certificate inline")
    {
        const auto options = load(directory,
            "serverCertificate = inline-certificate\n"
            "serverKey = inline-key\n");
        REQUIRE(options.getServerCertificate() == "inline-certificate");
        REQUIRE(options.getServerKey() == "inline-key");
    }
    SECTION("Server key and certificate from files, PEM intact")
    {
        // Trailing newline the way an editor or kubectl leaves it
        const auto certificatePath
            = directory.write("server.crt", certificate + "\n");
        const auto keyPath = directory.write("server.key", key + "\n");
        const auto options = load(directory,
            "serverCertificateFile = " + certificatePath.string() + "\n"
            "serverKeyFile = " + keyPath.string() + "\n");
        REQUIRE(options.getServerCertificate().starts_with(
                    "-----BEGIN CERTIFICATE-----\nMIIB\n"));
        REQUIRE(options.getServerKey().starts_with(
                    "-----BEGIN PRIVATE KEY-----\nMIIE\n"));
    }
    SECTION("Server key without certificate, and vice versa")
    {
        REQUIRE_THROWS_AS(load(directory, "serverKey = inline-key\n"),
                          std::invalid_argument);
        REQUIRE_THROWS_AS(
            load(directory, "serverCertificate = inline-certificate\n"),
            std::invalid_argument);
    }
    SECTION("Inline and file forms of the same setting conflict")
    {
        const auto keyPath = directory.write("server.key", key);
        REQUIRE_THROWS_AS(
            load(directory,
                 "serverCertificate = inline-certificate\n"
                 "serverKey = inline-key\n"
                 "serverKeyFile = " + keyPath.string() + "\n"),
            std::invalid_argument);
    }
    SECTION("A named secret file that does not exist")
    {
        REQUIRE_THROWS_AS(
            load(directory,
                 "serverCertificate = inline-certificate\n"
                 "serverKeyFile = " + (directory.path() / "nope.key").string()
                 + "\n"),
            std::invalid_argument);
    }
    SECTION("Access token and client certificate need TLS")
    {
        REQUIRE_THROWS_AS(load(directory, "accessToken = not-a-real-token\n"),
                          std::invalid_argument);
        REQUIRE_THROWS_AS(
            load(directory, "clientCertificate = inline-certificate\n"),
            std::invalid_argument);
    }
    SECTION("Access token and client certificate with TLS")
    {
        const auto tokenPath = directory.write("token", "not-a-real-token\n");
        const auto options = load(directory,
            "serverCertificate = inline-certificate\n"
            "serverKey = inline-key\n"
            "clientCertificate = client-certificate\n"
            "accessTokenFile = " + tokenPath.string() + "\n");
        REQUIRE(options.getClientCertificate() == "client-certificate");
        // Trailing newline from the file is not part of the token
        REQUIRE(options.getAccessToken() == "not-a-real-token");
    }
}
