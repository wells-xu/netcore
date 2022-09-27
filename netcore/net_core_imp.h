#pragma once
#include "netcore.h"
#include <thread>
#include <mutex>

namespace netcore {

class NetService : public INetService
{ 
public:
    NetService();
    ~NetService();

    virtual bool init();
    virtual bool close();
    virtual INetChannel* create_channel();
    virtual bool         remove_channel(INetChannel*);
private:
    void thread_proc();

    std::thread _thread;
    INetChannel* _channel;

    bool _is_started;
    std::mutex _main_mutex;
};

class NetChannel : public INetChannel
{
public:
    virtual bool init();

    virtual bool set_header(const std::string& header);
    virtual bool set_body();
    virtual bool set_cookie();

    typedef std::function<void(int type, void *data, void *data2, void *data3)> CallbackType;
    virtual bool post_request(const std::string &url, CallbackType callback, void *param, int timeout_ms = -1);
    virtual bool send_request(const std::string &url, void *param);

    virtual bool send_stop();
    virtual bool post_stop();
    virtual bool add_form_data(const std::string &name, const std::string &content);
    virtual bool add_form_file(const std::string &name, const std::string &content, const std::string &filename);

    void on_callback();
private:
    CallbackType _callback;
};
}
