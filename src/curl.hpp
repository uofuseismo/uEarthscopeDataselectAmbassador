#ifndef PRIVATE_CURL_HPP
#define PRIVATE_CURL_HPP
#include <exception>
#include <expected>
#include <memory>
#include <string>
#include <utility>
#include <spdlog/spdlog.h>
#include <spdlog/logger.h>
#include <curl/curl.h>
#if LIBCURL_VERSION_NUM < 0x080400
#error "libcurl >= 8.4.0 required: CURLOPT_MAXFILESIZE must apply to streamed responses"
#endif
#include "uEarthscopeDataselectAmbassador/curlGlobal.hpp"
#include "uEarthscopeDataselectAmbassador/earthscopeClientOptions.hpp"

namespace UEarthscopeDataselectAmbassador
{

struct HTTPResponse
{
    std::string body;        ///< miniSEED on 200, error text on 4xx/5xx
    std::string contentType; ///< e.g., application/vnd.fdsn.mseed
    long status{0};          ///< HTTP status code
};

[[nodiscard]] size_t writeToBuffer(char *ptr, const size_t size,
                                   const size_t nmemb, void *userp)
{
    auto *buffer = static_cast<std::string *> (userp);
    if (buffer == nullptr){return 0;}
    buffer->append(ptr, size*nmemb);
    return size*nmemb;
}

/*
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
*/

class CURLHandle
{
public:
    enum class Error
    {
        InvalidURL,        // Provided URL is empty
        InvalidCURLOption, // Error setting an option in cURL.
        Uninitialized,     // Class not initialized
        TimedOut,          // CURLE_OPERATION_TIMEDOUT
        ConnectionFailed,  // COULDNT_RESOLVE_HOST, COULDNT_CONNECT, SEND_ERROR,
                           // RECV_ERROR, GOT_NOTHING, PARTIAL_FILE
        TLSFailed,         // SSL_*, PEER_FAILED_VERIFICATION
        TooLarge,          // FILESIZE_EXCEEDED
        Other
    };
public:
    /// Passkey: public constructor, but only the factory can make a Key.
    class Key
    {
        friend class CURLFactory;
        Key() = default;
    };

    explicit CURLHandle(Key) :
        mCurl(curl_easy_init(),
              &curl_easy_cleanup)
    {
        if (!mCurl){throw std::runtime_error("Failed to initialize curl");}
        // If anything below throws, unique_ptr cleans up the handle
        setOption(CURLOPT_NOPROGRESS,
                  1L,
                  "Failed to set no progress");
        setOption(CURLOPT_WRITEFUNCTION,
                  &writeToBuffer,
                  "Failed to set write callback");
        setOption(CURLOPT_FOLLOWLOCATION, 1L, "Failed to set follow location");
    }

    /// Buckets a libcurl transfer error into a CURLHandle::Error.
    [[nodiscard]] 
    static CURLHandle::Error classifyCURLError(const CURLcode code) noexcept
    {
        switch (code)
        {
            case CURLE_OPERATION_TIMEDOUT:
                return CURLHandle::Error::TimedOut;
            case CURLE_FILESIZE_EXCEEDED:
                return CURLHandle::Error::TooLarge;
            // Couldn't reach the server, or the connection dropped mid-transfer
            case CURLE_COULDNT_RESOLVE_HOST:   // DNS
            case CURLE_COULDNT_CONNECT:        // refused / unreachable
            case CURLE_SEND_ERROR:
            case CURLE_RECV_ERROR:             // includes connection reset
            case CURLE_GOT_NOTHING:            // server closed without replying
            case CURLE_PARTIAL_FILE:           // body shorter than Content-Length
            case CURLE_WEIRD_SERVER_REPLY:     // garbage, e.g. a broken LB/proxy
            case CURLE_HTTP2:
            case CURLE_HTTP2_STREAM:           // h2 stream reset
            case CURLE_HTTP3:
            case CURLE_QUIC_CONNECT_ERROR:
                return CURLHandle::Error::ConnectionFailed;
            // TLS: handshake, the peer's certificate, or our own CA bundle
            case CURLE_SSL_CONNECT_ERROR:
            case CURLE_PEER_FAILED_VERIFICATION: // bad/expired cert, hostname mismatch
            case CURLE_SSL_CACERT_BADFILE:       // CA bundle missing from the container
            case CURLE_SSL_CERTPROBLEM:
            case CURLE_SSL_CIPHER:
            case CURLE_SSL_ISSUER_ERROR:
            case CURLE_SSL_CRL_BADFILE:
            case CURLE_SSL_PINNEDPUBKEYNOTMATCH:
            case CURLE_SSL_INVALIDCERTSTATUS:
            case CURLE_SSL_CLIENTCERT:
            case CURLE_SSL_SHUTDOWN_FAILED:
            case CURLE_SSL_ENGINE_NOTFOUND:
            case CURLE_SSL_ENGINE_SETFAILED:
            case CURLE_SSL_ENGINE_INITFAILED:
            case CURLE_ECH_REQUIRED:
                return CURLHandle::Error::TLSFailed;
            default:
                return CURLHandle::Error::Other;   // WRITE_ERROR, OUT_OF_MEMORY, ...
        }
    }       

    [[nodiscard]] 
    std::expected<HTTPResponse, Error> get(const std::string &url,
                                           spdlog::logger *logger)
    {
        if (url.empty())
        {
            return std::unexpected(Error::InvalidURL);
        }
        if (mCurl == nullptr)
        {   
            return std::unexpected(Error::Uninitialized);
        }   

        std::string buffer;
        std::array<char, CURL_ERROR_SIZE> errorBuffer{};

        try
        {
            setOption(CURLOPT_HTTPGET, 1L, "Failed to set GET"); // Defensive for when POST arrives
            setOption(CURLOPT_URL, url.c_str(), "Failed to set URL");
            setOption(CURLOPT_WRITEDATA, &buffer, "Failed to set write buffer");
            setOption(CURLOPT_ERRORBUFFER, errorBuffer.data(),
                      "Failed to set error buffer");
        }
        catch (const std::exception &e)
        {
            if (logger)
            {
                SPDLOG_LOGGER_ERROR(logger, "Failed to option {}",
                                    e.what());
            }
            return std::unexpected(Error::InvalidCURLOption);
        }

        const auto code = curl_easy_perform(mCurl.get());

        // Don't leave the handle pointing at stack memory
        curl_easy_setopt(mCurl.get(), CURLOPT_WRITEDATA, nullptr);
        curl_easy_setopt(mCurl.get(), CURLOPT_ERRORBUFFER, nullptr);

        if (code != CURLE_OK)
        {
            const auto error = CURLHandle::classifyCURLError(code);
            if (logger != nullptr)
            {
                std::string details{errorBuffer.data()};
                if (details.empty()){details = curl_easy_strerror(code);}
                if (error == Error::TLSFailed)
                {
                    SPDLOG_LOGGER_CRITICAL(logger, "TLS failure: {}", details);
                }
                else
                {
                    SPDLOG_LOGGER_WARN(logger, "Request failed: {}", details);
                }
            }
            return std::unexpected(error);
        }

        HTTPResponse response;
        if (curl_easy_getinfo(mCurl.get(),
                              CURLINFO_RESPONSE_CODE,
                              &response.status) != CURLE_OK)
        {
            if (logger)
            {
                SPDLOG_LOGGER_ERROR(logger, "Failed to get response code");
            }
            return std::unexpected(Error::Other);
        }
        // Owned by the handle; may be null (e.g., 204)
        const char *contentType{nullptr};
        if (curl_easy_getinfo(mCurl.get(), CURLINFO_CONTENT_TYPE, &contentType)
            == CURLE_OK && contentType != nullptr)
        {
            response.contentType = contentType;
        }
        response.body = std::move(buffer);
        return response;

    }

    void setRequestTimeOut(const std::chrono::milliseconds &timeOut)
    {
        setOption(CURLOPT_TIMEOUT_MS,
                  static_cast<long> (timeOut.count()),
                  "Failed to set cURL timeout");
    }

    void setConnectTimeOut(const std::chrono::milliseconds &timeOut)
    {
        setOption(CURLOPT_CONNECTTIMEOUT_MS,
                  static_cast<long> (timeOut.count()),
                  "Failed to set cURL connect timeout");
    }   

    void setMaximumResponseSizeInBytes(const int64_t maxResponseBytes)
    {
        setOption(CURLOPT_MAXFILESIZE_LARGE,
                  static_cast<curl_off_t> (maxResponseBytes),
                  "Failed to set max response size");
    }

    void setBearerAuth(const std::string &token)
    {   
        setOption(CURLOPT_HTTPAUTH,
                  CURLAUTH_BEARER,
                  "Failed to set bearer auth");
        setOption(CURLOPT_XOAUTH2_BEARER,
                  token.c_str(),
                  "Failed to set token");
    }   

    void setDigestAuth(
        const std::pair<std::string, std::string> &userNameAndPassword)
    {
        setOption(CURLOPT_HTTPAUTH,
                  CURLAUTH_DIGEST,
                  "Failed to HTTP digest authentication");
        setOption(CURLOPT_USERNAME, 
                  userNameAndPassword.first.c_str(),
                  "Failed to set user name");
        setOption(CURLOPT_PASSWORD, 
                  userNameAndPassword.second.c_str(),
                  "Failed to set password");
    }   

    CURLHandle(const CURLHandle &) = delete;
    CURLHandle& operator=(const CURLHandle &) = delete;
private:
    template<typename T>
    void setOption(const CURLoption option, T value, const char *message)
    {   
        if (curl_easy_setopt(mCurl.get(), option, value) != CURLE_OK)
        {
            throw std::runtime_error(message);
        }
    }   
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> mCurl;
};

class CURLFactory
{
public:
    CURLFactory(EarthscopeClientOptions options,
                const CURLGlobal &) :
        mOptions(options)
    {
    }

    [[nodiscard]] std::unique_ptr<CURLHandle>
        create(const EarthscopeClientOptions &options,
               const bool useToken) const
    {
        auto handle = std::make_unique<CURLHandle>(CURLHandle::Key{});
        handle->setConnectTimeOut(options.getConnectTimeOut());
        handle->setRequestTimeOut(options.getRequestTimeOut());
        handle->setMaximumResponseSizeInBytes(
            options.getMaximumResponseSizeInBytes());
        if (!useToken)
        {
            handle->setDigestAuth(options.getLegacyCredentials());
        }
        else
        {
            handle->setBearerAuth(options.getAccessToken());
        }
        return handle;
    }
 
//private:
    EarthscopeClientOptions mOptions;
};

}
#endif
