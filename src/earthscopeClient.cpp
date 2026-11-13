#include <array>
#include <cstddef>
#include <ios>
#include <memory>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <curl/curl.h>
#include <spdlog/spdlog.h>
#include <spdlog/logger.h>
//NOLINTNEXTLINE(misc-include-cleaner)
#include <spdlog/sinks/stdout_color_sinks.h>
#include "uEarthscopeDataselectAmbassador/earthscopeClient.hpp"
#include "uEarthscopeDataselectAmbassador/earthscopeClientOptions.hpp"

using namespace UEarthscopeDataselectAmbassador;

namespace
{

/// Callback function that writes data to std::ostream
[[nodiscard]] size_t writeCURLData(void *buf, const size_t size,
                                   const size_t nmemb, void *userp)
{
    if (userp)
    {
        std::ostream &os = *static_cast<std::ostream *>(userp);
        auto len = static_cast<std::streamsize> (size*nmemb);
        if (os.write(static_cast<char *> (buf), len)){return len;}
    }
    return 0;
}

struct HTTPResponse
{
    std::string body;        ///< miniSEED on 200, error text on 4xx/5xx
    std::string contentType; ///< e.g., application/vnd.fdsn.mseed
    long status{0};          ///< HTTP status code
};

struct WriteBuffer
{
    std::string data;
    size_t maxBytes{0};
    bool exceededLimit{false};
};

[[nodiscard]] size_t writeToBuffer(char *ptr, const size_t size,
                                   const size_t nmemb, void *userp)
{
    auto *buffer = static_cast<WriteBuffer *> (userp);
    if (buffer == nullptr){return 0;}
    const auto nBytes = size*nmemb;
    if (buffer->data.size() + nBytes > buffer->maxBytes)
    {
        buffer->exceededLimit = true;
        return 0; // Aborts the transfer with CURLE_WRITE_ERROR
    }
    buffer->data.append(ptr, nBytes);
    return nBytes;
}

class CURLImpl
{
public:
    explicit
    CURLImpl(const std::pair<std::string, std::string> &userNameAndPassword) :
        CURLImpl()
    {
        setBasicAuth(userNameAndPassword);  
    }

    explicit CURLImpl(const std::string &token) :
        CURLImpl()
    {
        setBearerAuth(token);
    }

    CURLImpl()
    {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        mCurl = curl_easy_init();
        if (!mCurl){throw std::runtime_error("Failed to initialize curl");}
        setOption(CURLOPT_NOPROGRESS, 1L, "Failed to set no progress");
        setOption(CURLOPT_WRITEFUNCTION, &writeCURLData,
                  "Failed to set callback");
        setOption(CURLOPT_FOLLOWLOCATION, 1L, "Failed to set follow location");
        setOption(CURLOPT_TIMEOUT, mTimeOut, "Failed to set timeout");
        setOption(CURLOPT_SSL_VERIFYPEER, 0L, "Failed to set verify peer");
    }
   
    [[nodiscard]] HTTPResponse get(const std::string &url)
    {
        if (url.empty()){throw std::invalid_argument("URL is empty");}
        if (mCurl == nullptr)
        {
            throw std::runtime_error("CURL handle is not initialized");
        }

        WriteBuffer buffer;
        buffer.maxBytes = mMaxResponseBytes;
        std::array<char, CURL_ERROR_SIZE> errorBuffer{};

        setOption(CURLOPT_HTTPGET, 1L, "Failed to set GET"); // Defensive for when POST arrives
        setOption(CURLOPT_URL, url.c_str(), "Failed to set URL");
        setOption(CURLOPT_WRITEDATA, &buffer, "Failed to set write buffer");
        setOption(CURLOPT_ERRORBUFFER, errorBuffer.data(),
                  "Failed to set error buffer");

        const auto code = curl_easy_perform(mCurl);

        // Don't leave the handle pointing at stack memory
        curl_easy_setopt(mCurl, CURLOPT_WRITEDATA, nullptr);
        curl_easy_setopt(mCurl, CURLOPT_ERRORBUFFER, nullptr);

        if (code != CURLE_OK)
        {
            if (code == CURLE_WRITE_ERROR && buffer.exceededLimit)
            {
                throw std::length_error("Response exceeded "
                                      + std::to_string(mMaxResponseBytes)
                                      + " bytes");
            }
            std::string details{errorBuffer.data()};
            if (details.empty()){details = curl_easy_strerror(code);}
            throw std::runtime_error("Request failed: " + details);
        }

        HTTPResponse response;
        if (curl_easy_getinfo(mCurl, CURLINFO_RESPONSE_CODE, &response.status)
            != CURLE_OK)
        {
            throw std::runtime_error("Failed to get response code");
        }
        char *contentType{nullptr}; // Owned by the handle; may be null (e.g., 204)
        if (curl_easy_getinfo(mCurl, CURLINFO_CONTENT_TYPE, &contentType)
            == CURLE_OK && contentType != nullptr)
        {
            response.contentType = contentType;
        }
        response.body = std::move(buffer.data);
        return response;
    }

/*
    /// Gets data from a URL and puts the result in a std::string as a buffer.
    [[nodiscard]] std::string get(const std::string &url)
    {
        if (url.empty())
        {
            throw std::invalid_argument("URL is empty");
        }
        std::ostringstream outputStream;
        auto code = curl_easy_setopt(mCurl, CURLOPT_FILE, &outputStream);
        if (code != CURLE_OK)
        {
            clear();
            throw std::runtime_error("Failed to set output stream handle");
        }
        code = curl_easy_setopt(mCurl, CURLOPT_URL, url.c_str());
        if (code != CURLE_OK)
        {
            clear();
            throw std::runtime_error("Failed to set URL");
        }
        code = curl_easy_perform(mCurl);
        if (code != CURLE_OK)
        {
            clear();
            auto errorMessage = std::string {curl_easy_strerror(code)};
            throw std::runtime_error("Failed to get data from: " + url
                                   + " CURL failed with: " + errorMessage);
        }
        return outputStream.str();
    }
*/
    /// Destructor
    ~CURLImpl()
    {
        clear();
        curl_global_cleanup();
    }
    /// Clears the curl handle
    void clear() noexcept
    {
        if (mCurl){curl_easy_cleanup(mCurl);}
        mCurl = nullptr;
    }
private:
    template<typename T>
    void setOption(const CURLoption option, T value, const char *message)
    {   
        if (curl_easy_setopt(mCurl, option, value) != CURLE_OK)
        {   
            throw std::runtime_error(message);
        }   
    }

    void setBearerAuth(const std::string &token)
    {
        setOption(CURLOPT_HTTPAUTH, CURLAUTH_BEARER, "Failed to set bearer auth");
        setOption(CURLOPT_XOAUTH2_BEARER, token.c_str(), "Failed to set token");
    }

    void setBasicAuth(
        const std::pair<std::string, std::string> &userNameAndPassword)
    {
        setOption(CURLOPT_HTTPAUTH, CURLAUTH_DIGEST, "Failed to set basic auth");
        setOption(CURLOPT_USERNAME, 
                  userNameAndPassword.first.c_str(),
                  "Failed to user name");
        setOption(CURLOPT_PASSWORD, 
                  userNameAndPassword.second.c_str(),
                  "Failed to set password");
    }

    CURLImpl(const CURLImpl &curl) = delete;
    CURLImpl(CURLImpl &&curl) noexcept = delete;
    CURLImpl& operator=(const CURLImpl &curl) = delete;
    CURLImpl& operator=(CURLImpl &&curl) noexcept = delete;

    /// CURL handle
    CURL *mCurl{nullptr};
    /// CURL times out after mTimeOut seconds
    long mTimeOut{120L};
    /// Max response time.
    size_t mMaxResponseBytes{8*1024*1024};
};

}

class EarthscopeClient::EarthscopeClientImpl
{
public:
    EarthscopeClientImpl(const EarthscopeClientOptions &options,
                         std::shared_ptr<spdlog::logger> logger) :
        mOptions(options),
        mLogger(std::move(logger))
    {
        if (!mOptions.hasAccessToken() &&
            !mOptions.hasLegacyCredentials())
        {
            throw std::invalid_argument("Client needs a token or credentials");
        }
        if (mLogger == nullptr)
        {   
            // NOLINTBEGIN(misc-include-cleaner)
            constexpr const char *loggerName{"EarthscopeConsole"};
            mLogger = spdlog::get(loggerName);
            if (mLogger == nullptr)
            {   
                mLogger = spdlog::stdout_color_mt(loggerName);
            }   
            // NOLINTEND(misc-include-cleaner)
        }   
        mInitialized = true;   
    }

    EarthscopeClientOptions mOptions;
    std::shared_ptr<spdlog::logger> mLogger{nullptr};
    bool mInitialized{false};
};

/// Constructor
EarthscopeClient::EarthscopeClient
(
    const EarthscopeClientOptions &options,
    std::shared_ptr<spdlog::logger> logger
) :
    pImpl(std::make_unique<EarthscopeClientImpl> (
            options, std::move(logger)))
{
}

/// Initialized?
bool EarthscopeClient::isInitialized() const noexcept
{
    return pImpl->mInitialized;
}

/// Destructor
EarthscopeClient::~EarthscopeClient() = default;
