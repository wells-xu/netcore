#include "stdafx.h"
#include "net_channel.h"

#include "net_service.h"

#include <base/log/logger.h>
#include <base/comm/immediate_crash.h>

namespace netcore {

//CurlHandleTraits
bool CurlHandleTraits::CloseHandle(Handle handle)
{
    if (IsHandleValid(handle)) {
        curl_easy_cleanup(handle);
    }

    return true;
}

//NetChannel defines
NetChannel::NetChannel()
{
}

bool NetChannel::init(NetService *host, NetChannel* chan)
{
    if (host == nullptr) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lg(_main_mutex);
        if (_is_processing) {
            return false;
        }

        if (chan != nullptr) {
            if (chan->get_handle() == nullptr) {
                IMMEDIATE_CRASH();
            }

            if (_net_handle.IsValid()) {
                _net_handle.Close();
            }

            _net_handle.Set(curl_easy_duphandle(chan->get_handle()));
        } else {
            _net_handle.Set(curl_easy_init());
        }
    }

    _host_service = host;
    return true;
}

bool NetChannel::set_header(const std::string& header)
{
    if (header.empty()) {
        return false;
    }
    
    if (is_running()) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lg(_main_mutex);
        _request_headers.Set(
            curl_slist_append(_request_headers.Take(), header.c_str()));
    }
    return true;
}

bool NetChannel::set_body(const std::string& body)
{
    if (body.empty()) {
        return false;
    }

    if (is_running()) {
        return false;
    }

    if (this->_mime_part.IsValid()) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lg(_main_mutex);
        _http_body = body;
    }
    return true;
}

bool NetChannel::set_cookie(const std::string& cookie)
{
    if (cookie.empty()) {
        return false;
    }

    if (is_running()) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lg(_main_mutex);
        this->_http_cookie = cookie;
    }
    return false;
}

bool NetChannel::set_mime_data(const std::string& part_name,
    const std::string& part_content, const std::string& part_type)
{
    if (part_name.empty() || part_content.empty()) {
        return false;
    }

    //Setting large data is memory consuming: one might consider using curl_mime_data_cb in such a case.
    if (part_content.size() > 512000) {
        IMMEDIATE_CRASH();
        return false;
    }

    if (is_running()) {
        return false;
    }

    CURLcode ret = CURLE_OK;
    {
        std::lock_guard<std::mutex> lg(_main_mutex);
        if (!this->_http_body.empty()) {
            return false;
        }

        if (!this->_mime_part.IsValid()) {
            _mime_part.Set(curl_mime_init(this->_net_handle.Get()));
            if (!_mime_part.IsValid()) {
                IMMEDIATE_CRASH();
            }
        }

        auto part = curl_mime_addpart(_mime_part.Get());
        //curl_mime_data sets a mime part's body content from memory data.
        //data points to the data that gets copied by this function.The storage may safely be reused after the call.
        ret = curl_mime_data(part, part_content.c_str(), part_content.size());
        if (ret != CURLE_OK) {
            return false;
        }
        if (!part_type.empty()) {
            ret = curl_mime_type(part, part_type.c_str());
            if (ret != CURLE_OK) {
                return false;
            }
        }
        ret = curl_mime_name(part, part_name.c_str());
    }
    return (ret == CURLE_OK);
}

bool NetChannel::set_mime_file(
    const std::string& part_name,
    const std::string& file_path, const std::string& remote_name,
    const std::string& part_type)
{
    if (part_name.empty() || file_path.empty() || remote_name.empty()) {
        return false;
    }

    if (is_running()) {
        return false;
    }

    CURLcode ret = CURLE_OK;
    {
        std::lock_guard<std::mutex> lg(_main_mutex);
        if (!this->_http_body.empty()) {
            return false;
        }

        if (!this->_mime_part.IsValid()) {
            _mime_part.Set(curl_mime_init(this->_net_handle.Get()));
            if (!_mime_part.IsValid()) {
                IMMEDIATE_CRASH();
            }
        }

        auto part = curl_mime_addpart(_mime_part.Get());
        ret = curl_mime_filedata(part, file_path.c_str());
        if (ret != CURLE_OK) {
            return false;
        }
        ret = curl_mime_filename(part, remote_name.c_str());
        if (ret != CURLE_OK) {
            return false;
        }
        if (!part_type.empty()) {
            ret = curl_mime_type(part, part_type.c_str());
            if (ret != CURLE_OK) {
                return false;
            }
        }
        ret = curl_mime_name(part, part_name.c_str());
    }
    return (ret == CURLE_OK);
}

void NetChannel::setup_curl_opts()
{
    {
        //must be protected here
        std::lock_guard<std::mutex> lg(_main_mutex);
        //http headers
        if (_request_headers.IsValid()) {
            curl_easy_setopt(this->get_handle(), CURLOPT_HTTPHEADER, _request_headers.Get());
        }

        //http body
        /* @CURLOPT_POSTFIELDS
        Pass a char * as parameter, pointing to the full data to send in an HTTP POST operation. You must make sure that the data is formatted the way you want the server to receive it. libcurl will not convert or encode it for you in any way. For example, the web server may assume that this data is URL encoded.
        The data pointed to is NOT copied by the library: as a consequence, it must be preserved by the calling application until the associated transfer finishes. This behavior can be changed (so libcurl does copy the data) by setting the CURLOPT_COPYPOSTFIELDS option.
        */
        if (!_http_body.empty()) {
            curl_easy_setopt(this->get_handle(), CURLOPT_POST, 1);
            curl_easy_setopt(this->get_handle(), CURLOPT_POSTFIELDSIZE, _http_body.size());
            curl_easy_setopt(this->get_handle(), CURLOPT_POSTFIELDS, _http_body.c_str());
        } else if (_mime_part.IsValid()) {
            //http mime forms
            curl_easy_setopt(get_handle(), CURLOPT_MIMEPOST, this->_mime_part.Get());
        }

        //http cookie
        /*
        Pass a pointer to a null-terminated string as parameter. It will be used to set a cookie in the HTTP request. The format of the string should be NAME=CONTENTS, where NAME is the cookie name and CONTENTS is what the cookie should contain.  
        If you need to set multiple cookies, set them all using a single option concatenated like this: "name1=content1; name2=content2;" etc.
        */
        if (!_http_cookie.empty()) {
            curl_easy_setopt(this->get_handle(), CURLOPT_COOKIE, _http_cookie.c_str());
        }
    }

    //low speed limit
    /* abort if slower than 10 bytes/sec during 30 seconds */
    curl_easy_setopt(this->get_handle(), CURLOPT_LOW_SPEED_TIME, 30L);
    curl_easy_setopt(this->get_handle(), CURLOPT_LOW_SPEED_LIMIT, 10L);
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

    setup_curl_opts();
    
    _host_service->post_request(std::bind(
        &NetService::on_channel_request, _host_service, this,
        url, callback, param, timeout_ms));
    return true;
}

bool NetChannel::send_request(
    const std::string& url,
    CallbackType callback,
    void* param,
    int timeout_ms)
{
    {
        std::lock_guard<std::mutex> lg(_main_mutex);
        if (_is_processing) {
            return false;
        }

        if (_wait_event != nullptr) {
            return false;
        }
        _wait_event = _host_service->borrow_event_shell();

        _is_processing = true;
    }

    setup_curl_opts();

    _host_service->post_request(std::bind(
        &NetService::on_channel_request, _host_service, this,
        url, callback, param, timeout_ms));

    baselog::trace("[ns] send_request waiting...");
    ::WaitForSingleObject(_wait_event->Get(), INFINITE);
    baselog::trace("[ns] send_request waiting done");
    if (!_host_service->restore_event_shell(_wait_event)) {
        IMMEDIATE_CRASH();
    }

    {
        std::lock_guard<std::mutex> lg(_main_mutex);
        _wait_event = nullptr;
    }
    this->reset_multi_thread();
    baselog::trace("[ns] send_request all done");
    return true;
}

void NetChannel::send_stop()
{
    bool is_event_borrowed = false;
    {
        std::lock_guard<std::mutex> lg(_main_mutex);
        if (!_is_processing) {
            return;
        }

        BASELOG_TRACE("send_stop wait_event= {}", (void*)_wait_event);
        if (_wait_event == nullptr) {
            is_event_borrowed = true;
            _wait_event = _host_service->borrow_event_shell();
        }
    }

    baselog::trace("[ns] send_stop posting...");
    _host_service->post_request(std::bind(
        &NetService::on_channel_close, this->_host_service, this));

    baselog::trace("[ns] send_stop waiting...");
    ::WaitForSingleObject(_wait_event->Get(), INFINITE);
    baselog::trace("[ns] send_stop waiting done");
    if (is_event_borrowed) {
        if (!_host_service->restore_event_shell(_wait_event)) {
            IMMEDIATE_CRASH();
        }
        std::lock_guard<std::mutex> lg(_main_mutex);
        _wait_event = nullptr;
    }

    this->reset_multi_thread();
    baselog::trace("[ns] send_stop all done");
}

void NetChannel::post_stop()
{
    if (!is_running()) {
        return;
    }

    _host_service->post_request(std::bind(
        &NetService::on_channel_close, this->_host_service, this));
}

HandleShell* NetChannel::get_wait_event()
{
    std::lock_guard<std::mutex> lg(_main_mutex);
    return _wait_event;
}

void NetChannel::reset_multi_thread()
{
    {
        std::lock_guard<std::mutex> lg(_main_mutex);
        _is_processing = false;
        curl_easy_reset(this->_net_handle.Get());
        _request_headers.Close();
        _mime_part.Close();
        _http_body.clear();
        _http_cookie.clear();
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

} //namespace netcore

