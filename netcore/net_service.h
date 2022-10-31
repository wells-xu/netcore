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
    HandleShell* env {nullptr};
    void* param {nullptr};
    NetChannel* chan {nullptr};
    int timeout_ms{ TIMEOUT_MS_DEFAULT };

    void reset() {
        url.clear();
        cb.swap(std::move(CallbackType()));
        param = nullptr;
        chan = nullptr;
        env = nullptr;
        timeout_ms = TIMEOUT_MS_DEFAULT;
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

    typedef std::function<bool(HandleShell*)> NSCallBack;
    typedef struct _TaskInfo {
        NSCallBack cb;
        HandleShell* env = nullptr;
    }TaskInfo;
    void send_request(NSCallBack callback);
    void post_request(NSCallBack callback);

    //callback functions
    bool on_channel_close(NetChannel* channel, HandleShell* env);
    bool on_channel_remove(NetChannel* channel, HandleShell* env);
    bool on_channel_request(NetChannel* chan,
        const std::string url, CallbackType cb,
        void* param, int timeout_ms, HandleShell *env);

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

    struct UserCallbackTask {
        std::string *data {nullptr};
        NetResultType delivered_type{ NetResultType::NRT_ONCB_NONE };
        LibcurlPrivateInfo* pri {nullptr};
    };
    void add_user_callback(UserCallbackTask uct);

    LibcurlPrivateInfo* borrow_private_info();
    bool restore_private_info(LibcurlPrivateInfo* ptr);
    HandleShell* borrow_event_shell();
    bool restore_event_shell(HandleShell* ptr);

    std::string* borrow_short_buffer();
    bool restore_short_buffer(std::string* ptr);
    std::string* borrow_wrote_buffer();
    bool restore_wrote_buffer(std::string* ptr);

    void do_user_pending_tasks(std::deque<UserCallbackTask> &tasks);

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

    base::DistribPoolThreadSafe<std::string> _short_buffer_pool;
    base::DistribPoolThreadSafe<std::string> _wrote_buffer_pool;
    std::mutex _user_mutex;
    std::condition_variable _user_loop_event;
    std::deque<UserCallbackTask> _pending_user_callbacks;
};

} //namespace netcore
