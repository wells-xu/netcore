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
    static bool BASE_EXPORT CloseHandle(HANDLE handle);

    // Returns true if the handle value is valid.
    static bool IsHandleValid(HANDLE handle) {
        return handle != nullptr;
    }

    // Returns NULL handle value.
    static HANDLE NullHandle() { return nullptr; }
};

class NetChannel : public INetChannel
{
public:
    virtual bool init();
    NetChannel();
    ~NetChannel() = default;

    virtual bool set_header(const std::string& header);
    virtual bool set_body();
    virtual bool set_cookie();

    //typedef std::function<void(int type, void *data, void *data2, void *data3)> CallbackType;
    virtual bool post_request(const std::string &url, CallbackType callback, void *param, int timeout_ms = -1);
    virtual bool send_request(const std::string &url, CallbackType callback, void *param);

    virtual void send_stop();
    virtual void post_stop();
    virtual bool add_form_data(const std::string &name, const std::string &content);
    virtual bool add_form_file(const std::string &name, const std::string &content, const std::string &filename);

    void on_callback();

    CURL* get_handle() {
        return _net_handle.get();
    }
    void set_host_service(NetService *host) {
        _host_service = host;
    }
    void reset();
private:
    bool is_running();

    bool on_net_request_within_service();
    NetChannel* on_net_response();

    using CurlScopedHandle =
        base::win::GenericScopedHandle<CurlHandleTraits, base::win::DummyVerifierTraits>;

    //CallbackType _callback;
    HandleShell *_wait_event {nullptr};
    CurlScopedHandle _net_handle;
    bool _is_inited = false;
    NetService* _host_service {nullptr};
    std::mutex _main_mutex;
    bool _is_processing{false};
};

} //namespace netcore

