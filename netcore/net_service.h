#pragma once

#include "net_channel.h"

#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>

#include <base/comm/distrib_pool.h>
#include <base/win/scoped_handle.h>

namespace netcore {

class HandleShell {
public:
    HandleShell();
    ~HandleShell();
    HandleShell(HandleShell&& hs) :
        _handle(std::move(hs._handle)) {
    }

    base::win::HandleTraits::Handle Get() {
        return _handle.get();
    }
private:
    base::win::ScopedHandle _handle;
};

class CurlMHandleTraits {
public:
    using Handle = CURLM*;

    CurlMHandleTraits() = delete;
    CurlMHandleTraits(const CurlMHandleTraits&) = delete;
    CurlMHandleTraits& operator=(const CurlMHandleTraits&) = delete;

    // Closes the handle.
    static bool BASE_EXPORT CloseHandle(Handle handle) {
        return (CURLM_OK == curl_multi_cleanup(handle));
    }

    // Returns true if the handle value is valid.
    static bool IsHandleValid(Handle handle) {
        return handle != nullptr;
    }

    // Returns NULL handle value.
    static Handle NullHandle() { return nullptr; }
};

struct LibcurlPrivateInfo
{
    std::string url;
    CallbackType cb;
    void* param {nullptr};
    NetChannel* chan {nullptr};
    int timeout_ms{ TIMEOUT_MS_DEFAULT };
    NetResultType delivered_type { NetResultType::NRT_ONCB_NONE };

    void reset() {
        url.clear();
        cb.swap(std::move(CallbackType()));
        param = nullptr;
        chan = nullptr;
        timeout_ms = TIMEOUT_MS_DEFAULT;
        delivered_type = NetResultType::NRT_ONCB_NONE;
    }
};

class NetService : public INetService
{ 
public:
    NetService();
    ~NetService();

    virtual bool init();
    virtual bool close();
    virtual INetChannel* create_channel();
    virtual INetChannel* create_clone_channel(INetChannel* chan);
    virtual void         remove_channel(INetChannel* chan);

    typedef std::function<bool()> NSCallBack;
    typedef struct _TaskInfo {
        NSCallBack cb;
        //INetChannel* chan{ nullptr };
    }TaskInfo;
    void send_request(NSCallBack callback);
    void post_request(NSCallBack callback);
    HandleShell* borrow_event_shell();
    bool restore_event_shell(HandleShell* ptr);

    //callback functions
    //bool on_chanel_send_stop(NetChannel* channel);
    //bool on_chanel_post_stop(NetChannel* channel);
    bool on_channel_close(NetChannel* channel);
    bool on_channel_remove(NetChannel* channel);
    bool on_channel_request(NetChannel* chan,
        const std::string url, CallbackType cb, void* param, int timeout_ms);

    //libcurl callbacks
    static size_t on_callback_curl_write(
        char* ptr, size_t size, size_t nmemb, void* userdata);
    static size_t on_callback_curl_head(
        char* buffer, size_t size, size_t nitems, void* userdata);
    static int on_callback_curl_progress(
        void* clientp,
        curl_off_t dltotal,
        curl_off_t dlnow,
        curl_off_t ultotal,
        curl_off_t ulnow);
private:
    void thread_proc();
    void user_thread_proc();
    void wake_up_event();

    void do_pending_task(std::deque<TaskInfo> &tasks);
    void do_finish_channel();

    void clean_channel(NetChannel* chan);

    LibcurlPrivateInfo* borrow_private_info();
    bool restore_private_info(LibcurlPrivateInfo* ptr);

    std::thread _thread;
    std::thread _user_thread;
    base::DistribPoolThreadSafe<NetChannel> _channel_pool;
    base::DistribPoolThreadSafe<HandleShell> _event_pool;
    base::DistribPoolThreadSafe<LibcurlPrivateInfo> _curl_private_pool;

    bool _is_stopped{ true };

    using CurlMScopedHandle =
        base::win::GenericScopedHandle<CurlMHandleTraits, base::win::DummyVerifierTraits>;

    CurlMScopedHandle _net_handle;
    std::mutex _main_mutex;
    std::deque<TaskInfo> _pending_tasks;

    std::mutex _user_mutex;
    std::condition_variable _user_loop_event;
    std::deque<LibcurlPrivateInfo*> _pending_user_callbacks;
};

} //namespace netcore
