#include "stdafx.h"
#include "net_core_imp.h"
#include <iostream>

#include <base/log/logger.h>

namespace netcore {
NetService::NetService() :
    _is_started(false)
{
}

NetService::~NetService()
{
}

bool NetService::init()
{
    {
		std::unique_lock<std::mutex> ul(_main_mutex);
        if (_is_started) {
            return true;
        }
		_is_started = true;
    }

    bool is_succ = false;
    if (baselog::initialize(baselog::log_sink::windebug_sink)) {
        _thread = std::move(std::thread(&NetService::thread_proc, this));
        baselog::info("net service started now...");
        is_succ = true;
    }

    {
		std::unique_lock<std::mutex> ul(_main_mutex);
		_is_started = is_succ;
    }

    return is_succ;
}

bool NetService::close()
{
    baselog::info("netcore service is closing...");
    auto ret = baselog::uninitialize();
    {
		std::unique_lock<std::mutex> ul(_main_mutex);
        if (!_is_started) {
            return false;
        }

        if (_thread.joinable()) {
            _thread.join();
        }
		_is_started = false;
    }

    return ret;
}

void NetService::thread_proc()
{
    while (1) {
        ::Sleep(5000);
        BASELOG_TRACE("netcore thread processing...");
        this->_channel->on_callback();
        break;
    }
}

INetChannel* NetService::create_channel()
{
    _channel = new NetChannel;
    std::cout << "[ns] create_channel" << std::endl;
    return _channel;
}

bool NetService::remove_channel(INetChannel* chan)
{
    std::cout << "[ns] create_channel" << std::endl;
    return nullptr;
}

//NetChannel defines
bool NetChannel::init()
{
    return false;
}

bool NetChannel::set_header(const std::string& header)
{ 
    return false;
}

bool NetChannel::set_body()
{
    return false;
}

bool NetChannel::set_cookie()
{
    return false;
}

bool NetChannel::post_request(
    const std::string &url,
    CallbackType callback,
    void *param,
    int timeout_ms)
{
    _callback = callback;
    return false;
}

bool NetChannel::send_request(
    const std::string& url,
    void* param)
{
    return false;
}

bool NetChannel::send_stop()
{
    return false;
}

bool NetChannel::post_stop()
{
    return false;
}

bool NetChannel::add_form_data(
    const std::string &name,
    const std::string &content)
{
    return false;
}

bool NetChannel::add_form_file(
    const std::string &name,
    const std::string &content,
    const std::string &filename)
{
    return false;
}

void NetChannel::on_callback()
{
    if (_callback) {
        _callback(0, nullptr, nullptr, nullptr);
    }
}

}
