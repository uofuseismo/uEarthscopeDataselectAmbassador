#ifndef TESTING_FAKE_SERVER_HPP
#define TESTING_FAKE_SERVER_HPP
#include <atomic>
#include <cstdint>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace Testing
{

/// A loopback HTTP server that mimics EarthScope's origin stalls: it accepts
/// a connection and never sends a byte.  In StallThenRespond mode the first
/// connection stalls and later ones get a small 200.
class FakeServer
{
public:
    enum class Mode
    {
        Stall,
        StallThenRespond
    };

    explicit FakeServer(const Mode mode) :
        mMode(mode)
    {
        mSocket = ::socket(AF_INET, SOCK_STREAM, 0);
        if (mSocket < 0){throw std::runtime_error("socket failed");}
        constexpr int enable{1};
        ::setsockopt(mSocket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = 0;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto *generic = reinterpret_cast<sockaddr *> (&address);
        socklen_t length{sizeof(address)};
        if (::bind(mSocket, generic, sizeof(address)) != 0 ||
            ::listen(mSocket, 8) != 0 ||
            ::getsockname(mSocket, generic, &length) != 0)
        {
            ::close(mSocket);
            throw std::runtime_error("Could not start fake server");
        }
        mPort = ntohs(address.sin_port);
        mThread = std::jthread([this](const std::stop_token &token)
        {
            serve(token);
        });
    }

    FakeServer(const FakeServer &) = delete;
    FakeServer& operator=(const FakeServer &) = delete;

    ~FakeServer()
    {
        mThread.request_stop();
        if (mThread.joinable()){mThread.join();}
        const std::scoped_lock lock{mMutex};
        for (const auto client : mClients){::close(client);}
        ::close(mSocket);
    }

    [[nodiscard]] std::string endpoint() const
    {
        return "http://127.0.0.1:" + std::to_string(mPort)
             + "/fdsnws/dataselect/1";
    }

    [[nodiscard]] int connections() const noexcept
    {
        return mConnections.load();
    }

    static constexpr const char *PAYLOAD{"abcd"};
private:
    void serve(const std::stop_token &token)
    {
        while (!token.stop_requested())
        {
            pollfd descriptor{mSocket, POLLIN, 0};
            if (::poll(&descriptor, 1, 50) <= 0){continue;}
            const int client = ::accept(mSocket, nullptr, nullptr);
            if (client < 0){continue;}
            const auto count = ++mConnections;
            if (mMode == Mode::StallThenRespond && count > 1)
            {
                respond(client);
                ::close(client);
                continue;
            }
            const std::scoped_lock lock{mMutex};
            mClients.push_back(client);
        }
    }

    static void respond(const int client)
    {
        std::string request;
        std::vector<char> buffer(4096);
        while (request.find("\r\n\r\n") == std::string::npos)
        {
            const auto received = ::recv(client, buffer.data(), buffer.size(), 0);
            if (received <= 0){return;}
            request.append(buffer.data(), static_cast<size_t> (received));
        }
        const std::string response{
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/vnd.fdsn.mseed\r\n"
            "Content-Length: 4\r\n"
            "Connection: close\r\n"
            "\r\n" + std::string {PAYLOAD}};
        static_cast<void> (::send(client, response.data(), response.size(), 0));
    }

    std::jthread mThread;
    mutable std::mutex mMutex;
    std::vector<int> mClients;
    std::atomic<int> mConnections{0};
    Mode mMode;
    int mSocket{-1};
    uint16_t mPort{0};
};

}
#endif
