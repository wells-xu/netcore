#pragma once
#include "netcore.h"
#include <thread>
#include <mutex>

#include <curl/curl.h>
#include <base/win/scoped_handle.h>

namespace netcore {

class NetService;
class HandleShell;

class CurlHandleTraits {
public:
    using Handle = CURL*;

    CurlHandleTraits() = delete;
    CurlHandleTraits(const CurlHandleTraits&) = delete;
    CurlHandleTraits& operator=(const CurlHandleTraits&) = delete;

    // Closes the handle.
    static bool BASE_EXPORT CloseHandle(Handle handle);

    // Returns true if the handle value is valid.
    static bool IsHandleValid(Handle handle) {
        return handle != nullptr;
    }

    // Returns NULL handle value.
    static Handle NullHandle() { return nullptr; }
};

class CurlHeaderTraits {
public:
    using Handle = curl_slist*;

    CurlHeaderTraits() = delete;
    CurlHeaderTraits(const CurlHandleTraits&) = delete;
    CurlHeaderTraits& operator=(const CurlHandleTraits&) = delete;

    // Closes the handle.
    static bool BASE_EXPORT CloseHandle(Handle handle) {
        if (IsHandleValid(handle)) {
            curl_slist_free_all(handle);
        }
        return true;
    }

    // Returns true if the handle value is valid.
    static bool IsHandleValid(Handle handle) {
        return handle != nullptr;
    }

    // Returns NULL handle value.
    static Handle NullHandle() { return nullptr; }
};

class CurlMimeTraits {
public:
    using Handle = curl_mime*;

    CurlMimeTraits() = delete;
    CurlMimeTraits(const CurlMimeTraits&) = delete;
    CurlMimeTraits& operator=(const CurlMimeTraits&) = delete;

    // Closes the handle.
    static bool BASE_EXPORT CloseHandle(Handle handle) {
        if (IsHandleValid(handle)) {
            curl_mime_free(handle);
        }
        return true;
    }

    // Returns true if the handle value is valid.
    static bool IsHandleValid(Handle handle) {
        return handle != nullptr;
    }

    // Returns NULL handle value.
    static Handle NullHandle() { return nullptr; }
};

class NetChannel : public INetChannel
{
public:
    NetChannel();
    ~NetChannel() = default;

    bool init(NetService* host, NetChannel *chan = nullptr);

    virtual bool set_header(const std::string& header);
    virtual bool set_body(const std::string& header);
    virtual bool set_cookie(const std::string& cookie);

    virtual bool post_request(const std::string& url,
        CallbackType callback = 0, void* context = 0,
        int timeout_ms = TIMEOUT_MS_INFINITE);
    virtual bool send_request(const std::string& url,
        CallbackType callback = 0, void *context = 0,
        int timeout_ms = TIMEOUT_MS_INFINITE);

    virtual void send_stop();
    virtual void post_stop();
    virtual bool set_mime_data(const std::string& part_name,
        const std::string& part_content, const std::string& part_type = "");
    virtual bool set_mime_file(const std::string& part_name,
        const std::string& file_path, const std::string& remote_name,
        const std::string& part_type = "");

    //void on_callback(NetCallbackType type, void* callback_param);

    CURL* get_handle() {
        return _net_handle.get();
    }
    HandleShell* get_wait_event();
    void reset_multi_thread();
    void reset_for_distrib_pool() {}
private:
    bool is_running();

    bool on_net_request_within_service();
    NetChannel* on_net_response();

    void setup_curl_opts();

    //CallbackType _callback;
    HandleShell* _wait_event{ nullptr };
    NetService* _host_service {nullptr};
    std::mutex _main_mutex;
    bool _is_inited = false;
    bool _is_processing{false};

    using CurlScopedHandle =
        base::win::GenericScopedHandle<CurlHandleTraits, base::win::DummyVerifierTraits>; 
    using CurlHeaderHandle =
        base::win::GenericScopedHandle<CurlHeaderTraits, base::win::DummyVerifierTraits>;
    using CurlMimeHandle =
        base::win::GenericScopedHandle<CurlMimeTraits, base::win::DummyVerifierTraits>;
    //libcurl stuff
    CurlScopedHandle _net_handle;
    CurlHeaderHandle _request_headers;
    CurlMimeHandle _mime_part;
    std::string _http_body;
    std::string _http_cookie;
};

} //namespace netcore

