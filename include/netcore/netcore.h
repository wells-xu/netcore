// netcore.cpp : Defines the exported functions for the DLL application.
//
#pragma once

#include <string>
#include <functional>
 
#ifdef NETCORE_EXPORTS
#define DLL_NETCORE_API extern "C" __declspec(dllexport)
#else
#define DLL_NETCORE_API __declspec(dllimport)
#endif

namespace netcore {

class INetService;
class INetChannel;

DLL_NETCORE_API bool         net_service_startup();
DLL_NETCORE_API INetService* net_service_instance();
DLL_NETCORE_API bool         net_service_shutdown(INetService*);

static const int TIMEOUT_MS_INFINITE = 0;
static const int TIMEOUT_MS_DEFAULT = 20000;
static const std::int64_t kContentLengthUnkown = -1;

//codes from: https://curl.se/libcurl/c/libcurl-errors.html
enum class NetResultCode {
    //All fine. Proceed as usual.
    CURLE_OK = 0,
    //The URL you passed to libcurl used a protocol that this libcurl does not support.The support might be a compile - time option that you did not use, it can be a misspelled protocol string or just a protocol libcurl has no code for.
    CURLE_UNSUPPORTED_PROTOCOL = 1,
    //Early initialization code failed.This is likely to be an internal error or problem, or a resource problem where something fundamental could not get done at init time.
    CURLE_FAILED_INIT = 2,
    //The URL was not properly formatted.
    CURLE_URL_MALFORMAT = 3,
    //A requested feature, protocol or option was not found built - in in this libcurl due to a build - time decision.This means that a feature or option was not enabled or explicitly disabled when libcurl was built and in order to get it to function you have to get a rebuilt libcurl.
    CURLE_NOT_BUILT_IN = 4,
    //Could not resolve proxy. The given proxy host could not be resolved.
    CURLE_COULDNT_RESOLVE_PROXY = 5,
    //Could not resolve host.The given remote host was not resolved.
    CURLE_COULDNT_RESOLVE_HOST = 6,
    //Failed to connect() to host or proxy.
    CURLE_COULDNT_CONNECT = 7,
    //We were denied access to the resource given in the URL.For FTP, this occurs while trying to change to the remote directory.
    CURLE_REMOTE_ACCESS_DENIED = 9,
    //A problem was detected in the HTTP2 framing layer.This is somewhat genericand can be one out of several problems, see the error buffer for details.
    CURLE_HTTP2 = 16,
    //A file transfer was shorter or larger than expected.This happens when the server first reports an expected transfer size, and then delivers data that does not match the previously given size.
    CURLE_PARTIAL_FILE = 18,
    //An error occurred when writing received data to a local file, or an error was returned to libcurl from a write callback.
    CURLE_WRITE_ERROR = 23,
    //Failed starting the upload.For FTP, the server typically denied the STOR command.The error buffer usually contains the server's explanation for this.
    CURLE_UPLOAD_FAILED = 25,
    //There was a problem reading a local file or an error returned by the read callback.
    CURLE_READ_ERROR = 26,
    //A memory allocation request failed.This is serious badness and things are severely screwed up if this ever occurs.
    CURLE_OUT_OF_MEMORY = 27, 
    //Operation timeout.The specified time - out period was reached according to the conditions.
    CURLE_OPERATION_TIMEDOUT = 28,
    //The server does not support or accept range requests.
    CURLE_RANGE_ERROR = 33, 
    //This is an odd error that mainly occurs due to internal confusion.
    CURLE_HTTP_POST_ERROR = 34,
    //A problem occurred somewhere in the SSL / TLS handshake.You really want the error buffer and read the message there as it pinpoints the problem slightly more.Could be certificates(file formats, paths, permissions), passwords, and others.
    CURLE_SSL_CONNECT_ERROR = 35,
    //The download could not be resumed because the specified offset was out of the file boundary.
    CURLE_BAD_DOWNLOAD_RESUME = 36,
    //Too many redirects.When following redirects, libcurl hit the maximum amount.Set your limit with CURLOPT_MAXREDIRS.
    CURLE_TOO_MANY_REDIRECTS = 47,
    //An option passed to libcurl is not recognized / known.Refer to the appropriate documentation.This is most likely a problem in the program that uses libcurl.The error buffer might contain more specific information about which exact option it concerns.
    CURLE_UNKNOWN_OPTION = 48,
    //failed sending network data
    CURLE_SEND_ERROR = 55,
    //failure in receiving network data
    CURLE_RECV_ERROR = 56,
    //Unrecognized transfer encoding.
    CURLE_BAD_CONTENT_ENCODING = 61,
    //Proxy handshake error.CURLINFO_PROXY_ERROR provides extra details on the specific problem.
    CURLE_PROXY = 97, 
    //Canceled request by user
    CURLE_CANCEL_BY_USER = 400, 
    //All other error type
    CURLE_UNKOWN_ERROR = 0x8000,
};

struct NetResultHeader {
    const char* data = nullptr;
    std::int64_t data_len = 0;
};

struct NetResultProgress {
    std::int64_t download_total_size = 0;
    std::int64_t download_transfered_size = 0;
    std::int64_t upload_total_size = 0;
    std::int64_t upload_transfered_size = 0;
    std::uint64_t startup_time = 0;
    std::uint64_t current_time = 0;
    std::double_t download_speed = 0.0;
    std::double_t upload_speed = 0.0;
};

struct NetResultWrite {
    const char* data = nullptr;
    std::int64_t data_len = 0;
};

struct NetResultFinish {
    //result code
    NetResultCode result_code{ NetResultCode::CURLE_UNKOWN_ERROR };
    // http request url
    const char* request_url = nullptr;
    //transfered body data
    const char* data = nullptr;
    std::int64_t data_len = 0;
    //header data
    const char* http_header = nullptr;
    std::int64_t http_header_len = 0;
    //http metrics
    std::int64_t http_response_code = 0;
    //http version: CURL_HTTP_VERSION_1_1 CURL_HTTP_VERSION_2_0
    long http_version = 0; 
    // The total time in microseconds for the previous transfer, including name resolving, TCP connect etc. The curl_off_t represents the time in microseconds.
    std::int64_t total_time_ms = 0;
    // The total time in microseconds  from the start until the name resolving was completed.
    std::int64_t namelookup_time_ms = 0;
    // The total time in microseconds from the start until the connection to the remote host(or proxy) was completed.
    std::int64_t connected_time_ms = 0;
    // The time, in microseconds, it took from the start until the SSL/SSH connect/handshake to the remote host was completed. 
    std::int64_t app_connected_time_ms = 0;
    // The time, in microseconds, it took from the start until the file transfer is just about to begin.
    std::int64_t pretransfer_time_ms = 0;
    // The time, in microseconds, it took from the start until the first byte is received by libcurl.
    std::int64_t starttransfer_time_ms = 0;
    // The total time, in microseconds, it took for all redirection steps include name lookup, connect, pretransfer and transfer before final transaction was started.
    std::int64_t redirect_time_ms = 0;
    // The total number of redirections that were actually followed.
    long         redirect_count = 0;
    // The average download speed that curl measured for the complete download. Measured in bytes/second.
    std::int64_t download_speed_bytes_persecond = 0;
    // The average upload speed that curl measured for the complete upload. Measured in bytes/second.
    std::int64_t upload_speed_bytes_persecond = 0;
     // The total amount of bytes that were uploaded.
    std::int64_t uploaded_size_bytes = 0;
    // The total amount of bytes that were downloaded. All meta and header data are excluded and will not be counted in this number. 
    std::int64_t playload_size_bytes = 0;
    // The specified size of the upload. Stores -1 if the size is not known.
    std::int64_t content_length_upload = kContentLengthUnkown;
    // The content-length of the download. This is the value read from the Content-Length: field. Stores -1 if the size is not known.
    std::int64_t content_length_download = kContentLengthUnkown;
    // The content-type of the downloaded object. This is the value read from the Content-Type: field. If you get NULL, it means that the server did not send a valid Content-Type header or that the protocol used does not support this.
    // Pointing to private memory you MUST NOT free it.
    // You MUST copy && store it youself if you wanted to use it after callback finished.
    const char* http_content_type = nullptr;
    // The pointer to a null - terminated string holding the IP address of the most recent connection done with this curl handle.
    // Pointing to private memory you MUST NOT free it.
    // You MUST copy && store it youself if you wanted to use it after callback finished.
    const char* primary_ip_string = nullptr;
    // The destination port of the most recent connection done with this curl handle.
    // This is the destination port of the actual TCP or UDP connection libcurl used. If a proxy was used for the most recent transfer, this is the port number of the proxy, if no proxy was used it is the port number of the most recently accessed URL.
    long primary_port = 0;
    // The pointer to a null - terminated string holding the IP address of the local end of most recent connection done with this curl handle.
    // Pointing to private memory you MUST NOT free it.
    // You MUST copy && store it youself if you wanted to use it after callback finished.
    const char* local_ip_string = nullptr;
    // The local port number of the most recent connection done with this curl handle.
    long local_port = 0;
    // The pointer to a null-terminated string holding the URL scheme used for the most recent connection done with this CURL handle.
    // Pointing to private memory you MUST NOT free it.
    // You MUST copy && store it youself if you wanted to use it after callback finished.
    const char* scheme_type = nullptr;
    // The pointer to a null-terminated string holding the referrer header.
    // Pointing to private memory you MUST NOT free it.
    // You MUST copy && store it youself if you wanted to use it after callback finished.
    const char* referrer_header = nullptr;
    // The pointer to get the last used effective URL.
    // In cases when you have asked libcurl to follow redirects, it may not be the same value you set with CURLOPT_URL.
    // Pointing to private memory you MUST NOT free it.
    // You MUST copy && store it youself if you wanted to use it after callback finished.
    const char* last_effective_url = nullptr;
    // The pointer to a char pointer and get the last used effective HTTP method.
    // In cases when you have asked libcurl to follow redirects, the method may not be the same method the first request would use.
    // Pointing to private memory you MUST NOT free it.
    // You MUST copy && store it youself if you wanted to use it after callback finished.
    const char* last_effective_method = nullptr;
    // The average speed that from oncallbacks progress with app itself 
    std::double_t app_average_speed = 0.0;
};

enum class NetResultType {
    NRT_ONCB_NONE = 0,
    NRT_ONCB_HEADER = 1,
    NRT_ONCB_PROGRESS = 2,
    NRT_ONCB_WRITE = 4,
    NRT_ONCB_FINISH = 8,
};

#define CHECK_NET_RESULT_TYPE_I(v, t, i) \
    (static_cast<i>(v) & static_cast<i>(t))
#define CHECK_NET_RESULT_TYPE(v, t) CHECK_NET_RESULT_TYPE_I(v, t, std::uint32_t)
#define IS_NET_RESULT_TYPE_CONTAIN(v, t) (static_cast<std::uint32_t>(t) == CHECK_NET_RESULT_TYPE(v, t))

typedef std::function<void(NetResultType type, void* data, void* context)> CallbackType;

class INetService {
public:
    virtual bool init() = 0;
    virtual bool close() = 0;

    virtual INetChannel* create_channel() = 0;
    virtual INetChannel* create_clone_channel(INetChannel*) = 0;
    virtual void         remove_channel(INetChannel*) = 0;
};

class INetChannel {
public:
    virtual bool set_header(const std::string& header) = 0;
    virtual bool set_body(const std::string& body) = 0;
    virtual bool set_cookie(const std::string& cookie) = 0;
    virtual bool set_mime_data(const std::string &part_name_utf8,
        const std::string &file_data_utf8, const std::string &part_type = "") = 0;
    virtual bool set_mime_file(const std::string &part_name_utf8,
        const std::string &file_path_utf8, const std::string &remote_name_utf8,
        const std::string &part_type = "") = 0;

    virtual void enable_callback(NetResultType nrt) = 0;
    virtual void disable_callback(NetResultType nrt) = 0;

    virtual bool post_request(const std::string &url,
        CallbackType callback = 0, void * userdata = 0,
        int timeout_ms = TIMEOUT_MS_INFINITE) = 0;
    virtual bool send_request(const std::string &url,
        CallbackType callback = 0, void *context = 0,
        int timeout_ms = TIMEOUT_MS_INFINITE) = 0;

    virtual void send_stop() = 0;
    virtual void post_stop() = 0;
};

}
