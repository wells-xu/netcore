#pragma once

#include <thread>
#include <queue>
#include <string>
#include <mutex>
#include <atomic>

#include <base/files/file_loadder.h>
#include <base/files/file_path.h>

class NetcoreMonkeyTest
{
public:
    static NetcoreMonkeyTest& Instance();
    bool init(const base::FilePath& path);
    bool run();
    bool stop();
private:
    void thrad_func_create();
    void thrad_func_request();
    void thrad_func_stop();
    void thrad_func_remove();
    
    std::thread _net_thread_create;
    std::thread _net_thread_request;
    std::thread _net_thread_stop;
    std::thread _net_thread_remove;

    base::FileLoadder<base::TextLineProc> _file_loadder;

    std::mutex _mutex;
    bool _is_stopped = true;
    std::deque<void*> _channel_pool;
};

