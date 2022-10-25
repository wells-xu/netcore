// netcore.cpp : Defines the exported functions for the DLL application.
//
#pragma once

#include "stdafx.h"
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

static const int TIMEOUT_MS_INFINITE = -1;
static const int TIMEOUT_MS_DEFAULT = 20000;
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
    //Unrecognized transfer encoding.
    CURLE_BAD_CONTENT_ENCODING = 61,
    //Proxy handshake error.CURLINFO_PROXY_ERROR provides extra details on the specific problem.
    CURLE_PROXY = 97, 
    //All other error type
    CURLE_UNKOWN_ERROR = 0x800,
};

struct NetResultHeader {
    NetResultCode result_code{ NetResultCode::CURLE_UNKOWN_ERROR };
    const char* content = nullptr;
    std::int64_t content_len = 0;
};

struct NetResultProgress {
    NetResultCode result_code{ NetResultCode::CURLE_UNKOWN_ERROR };
    std::int64_t total_size = 0;
    std::int64_t received_size = 0;
    std::uint32_t speed = 0;
};

struct NetResultWrite {
    NetResultCode result_code{ NetResultCode::CURLE_UNKOWN_ERROR };
    const char* content = nullptr;
    std::int64_t content_len = 0;
};

struct NetResultFinish {
    NetResultCode result_code{ NetResultCode::CURLE_UNKOWN_ERROR };
    int http_status_code = 0;
    const char* http_content = nullptr;
    std::int64_t http_content_len = 0;
    const char* http_header = nullptr;
    std::int64_t http_header_len = 0;
    std::int64_t whole_time_ms = 0;
    std::uint32_t average_speed = 0;
    double nslookupSeconds = 0.0;
    double connectSeconds = 0.0;
    double pretransferSeconds = 0.0;
    double startTransferSeconds = 0.0;
    double handshakeSeconds = 0.0;
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
        int timeout_ms = TIMEOUT_MS_DEFAULT) = 0;
    virtual bool send_request(const std::string &url,
        CallbackType callback = 0, void *context = 0,
        int timeout_ms = TIMEOUT_MS_DEFAULT) = 0;

    virtual void send_stop() = 0;
    virtual void post_stop() = 0;

    //virtual void on_callback() = 0;
};

}
