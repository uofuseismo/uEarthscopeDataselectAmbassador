#ifndef PRIVATE_CURL_HPP
#define PRIVATE_CURL_HPP
#include <exception>
#include <memory>
#include <string>
#include <utility>
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

struct WriteBuffer
{
    std::string data;
    size_t maxBytes{0};
    bool exceededLimit{false};
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

    [[nodiscard]] HTTPResponse get(const std::string &url)
    {
        if (url.empty()){throw std::invalid_argument("URL is empty");}
        if (mCurl == nullptr)
        {   
            throw std::runtime_error("CURL handle is not initialized");
        }   

        WriteBuffer buffer;
        std::array<char, CURL_ERROR_SIZE> errorBuffer{};

        setOption(CURLOPT_HTTPGET, 1L, "Failed to set GET"); // Defensive for when POST arrives
        setOption(CURLOPT_URL, url.c_str(), "Failed to set URL");
        setOption(CURLOPT_WRITEDATA, &buffer, "Failed to set write buffer");
        setOption(CURLOPT_ERRORBUFFER, errorBuffer.data(),
                  "Failed to set error buffer");

        const auto code = curl_easy_perform(mCurl.get());

        // Don't leave the handle pointing at stack memory
        curl_easy_setopt(mCurl.get(), CURLOPT_WRITEDATA, nullptr);
        curl_easy_setopt(mCurl.get(), CURLOPT_ERRORBUFFER, nullptr);

        if (code != CURLE_OK)
        {
            if (code == CURLE_FILESIZE_EXCEEDED)
            {
                throw std::length_error("Response exceeded maximum size");
            }
            std::string details{errorBuffer.data()};
            if (details.empty()){details = curl_easy_strerror(code);}
            throw std::runtime_error("Request failed: " + details);
        }

        HTTPResponse response;
        if (curl_easy_getinfo(mCurl.get(),
                              CURLINFO_RESPONSE_CODE,
                              &response.status) != CURLE_OK)
        {
            throw std::runtime_error("Failed to get response code");
        }
        // Owned by the handle; may be null (e.g., 204)
        const char *contentType{nullptr};
        if (curl_easy_getinfo(mCurl.get(), CURLINFO_CONTENT_TYPE, &contentType)
            == CURLE_OK && contentType != nullptr)
        {
            response.contentType = contentType;
        }
        response.body = std::move(buffer.data);
        return response;

    }

    void setTimeout(const std::chrono::milliseconds &timeOut)
    {
        setOption(CURLOPT_TIMEOUT,
                  static_cast<long> (timeOut.count()),
                  "Failed to set cURL timeout");
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
        create(const EarthscopeClientOptions &options) const
    {
        auto handle = std::make_unique<CURLHandle>(CURLHandle::Key{});
        handle->setTimeout(options.getRequestTimeOut());
        handle->setMaximumResponseSizeInBytes(
            options.getMaximumResponseSizeInBytes());
        if (options.hasLegacyCredentials())
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
