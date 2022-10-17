#include "stdafx.h"
#include "net_channel.h"

#include "net_service.h"

#include <base/log/logger.h>
#include <base/comm/immediate_crash.h>

namespace netcore {

//CurlHandleTraits
bool CurlHandleTraits::CloseHandle(HANDLE handle)
{
    if (IsHandleValid(handle)) {
        curl_easy_cleanup(handle);
    }

    return true;
}

//NetChannel defines
NetChannel::NetChannel() :
    _net_handle(curl_easy_init())
{
}

bool NetChannel::init()
{
    if (is_running()) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lg(_main_mutex);
        if (_is_inited) {
            return false;
        }

        _is_inited = true;
    }

    if (!_net_handle.IsValid()) {
        baselog::fatal("[ns] net channel init failed");
        return false;
    }

    return true;
}

bool NetChannel::set_header(const std::string& header)
{
    if (is_running()) {
        return false;
    }

    return true;
}

bool NetChannel::set_body()
{
    if (is_running()) {
        return false;
    }

    return false;
}

bool NetChannel::set_cookie()
{
    if (is_running()) {
        return false;
    }

    return false;
}

bool NetChannel::post_request(
    const std::string &url,
    CallbackType callback,
    void *param,
    int timeout_ms)
{
    {
        std::lock_guard<std::mutex> lg(_main_mutex);
        if (_is_processing) {
            return false;
        }

        _is_processing = true;
    }

    {
        //auto pri_ptr = PrivateBufThreadSafe().borrow();
        void* pri_ptr = nullptr;
        curl_easy_setopt(this->get_handle(), CURLOPT_URL, "https://macx.net");
    }
    _host_service->post_request(std::bind(
        &NetService::on_channel_request, _host_service, this));
    return true;
}

bool NetChannel::send_request(
    const std::string& url,
    CallbackType callback, void* param)
{
    {
        std::lock_guard<std::mutex> lg(_main_mutex);
        if (_is_processing) {
            return false;
        }

        _is_processing = true;
    }

    return false;
}

void NetChannel::send_stop()
{
    {
        std::lock_guard<std::mutex> lg(_main_mutex);
        if (!_is_processing) {
            return;
        }

        if (_wait_event == nullptr) {
            _wait_event = _host_service->borrow_event_shell();
        }
    }

    _host_service->post_request(std::bind(
        &NetService::on_close_channel, this->_host_service,
        reinterpret_cast<void*>(this)));

    ::WaitForSingleObject(_wait_event->_handle.get(), INFINITE);
}

void NetChannel::post_stop()
{
    if (!is_running()) {
        return;
    }

    _host_service->post_request(std::bind(
        &NetService::on_close_channel, this->_host_service,
        reinterpret_cast<void*>(this)));
}

bool NetChannel::add_form_data(
    const std::string &name,
    const std::string &content)
{
    if (is_running()) {
        return false;
    }

    return false;
}

bool NetChannel::add_form_file(
    const std::string &name,
    const std::string &content,
    const std::string &filename)
{
    if (is_running()) {
        return false;
    }
    return false;
}

void NetChannel::reset()
{
    if (!this->is_running()) {
        IMMEDIATE_CRASH();
    }
}

bool NetChannel::is_running()
{
    std::lock_guard<std::mutex> lg(_main_mutex);
    return _is_processing;
}

NetChannel* NetChannel::on_net_response()
{
    return this;
}

bool NetChannel::on_net_request_within_service()
{
    return false;
}

void NetChannel::on_callback()
{
    //if (_callback) {
    //    _callback(0, nullptr, nullptr, nullptr);
    //}
}

} //namespace netcore

