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
    base::win::ScopedHandle _handle;
};


class NetService : public INetService
{ 
public:
    NetService();
    ~NetService();

    virtual bool init();
    virtual bool close();
    virtual INetChannel* create_channel();
    virtual void         remove_channel(INetChannel*);

    typedef std::function<bool()> NSCallBack;
    typedef struct _TaskInfo {
        NSCallBack cb;
        //INetChannel* chan{ nullptr };
    }TaskInfo;
    void send_request(NSCallBack callback);
    void post_request(NSCallBack callback);
    HandleShell* borrow_event_shell();

    //callback functions
    bool on_close_channel(void* param);
    bool on_channel_request(NetChannel* chan);

    //libcurl callbacks
    static size_t on_callback_curl_write(
        char* ptr, size_t size, size_t nmemb, void* userdata);
private:
    void thread_proc();
    void wake_up_event();

    void do_pending_task_threadin(std::deque<TaskInfo> &tasks);
    void do_finish_channel_threadin();

    std::thread _thread;
    base::DistribPoolThreadSafe<NetChannel> _channel_pool;
    base::DistribPoolThreadSafe<HandleShell> _event_pool;

    bool _is_stopped{ true };

    CURLM* _net_handle;
    std::mutex _main_mutex;
    std::condition_variable _main_loop_event;

    std::deque<TaskInfo> _pending_tasks;
};

} //namespace netcore
