#include "stdafx.h"
#include "net_core_imp.h"
#include <iostream>

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
    std::cout << "[ns] init" << std::endl;
    {
		std::unique_lock<std::mutex> ul(_main_mutex);
        if (_is_started) {
            return false;
        }
		_thread = std::move(std::thread(&NetService::thread_proc, this));
		_is_started = true;
    }
    return true;
}

bool NetService::close()
{
    std::cout << "[ns] close" << std::endl;
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

    return false;
}

void NetService::thread_proc()
{
    while (1) {
        ::Sleep(5000);
        std::cout << "[ns] thread processing..." << std::endl;
        this->_channel->on_callback();
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
