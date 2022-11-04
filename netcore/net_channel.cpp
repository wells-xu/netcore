#include "stdafx.h"
#include "net_channel.h"

#include "net_service.h"

#include <base/log/logger.h>
#include <base/comm/immediate_crash.h>
#include <base/time/time.h>

namespace netcore {

//CurlHandleTraits
bool CurlHandleTraits::CloseHandle(Handle handle)
{
    if (IsHandleValid(handle)) {
        baselog::trace("[ns] CloseHandle closing handle: {}", (void*)handle);
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

        if (_host_service != nullptr) {
            return false;
        }
    }

    if (chan != nullptr) {
        if (chan->get_handle() == nullptr) {
            IMMEDIATE_CRASH();
        }

        if (_net_handle.IsValid()) {
            _net_handle.Close();
        }

        _net_handle.Set(curl_easy_duphandle(chan->get_handle()));
    } else if (!_net_handle.IsValid()) {
        _net_handle.Set(curl_easy_init());
    }
    
    baselog::info("[ns] libcurl easy handle: {}", (void*)_net_handle.Get());
    _host_service = host;
    return true;
}

bool NetChannel::set_header(const std::string& header)
{
    if (header.empty()) {
        return false;
    }
    
    {
        std::lock_guard<std::mutex> lg(_main_mutex);
        if (_is_processing) {
            return false;
        }

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

    {
        std::lock_guard<std::mutex> lg(_main_mutex);
        if (_is_processing) {
            return false;
        }

        if (this->_mime_part.IsValid()) {
            return false;
        }
        _http_body = body;
    }

    return true;
}

bool NetChannel::set_cookie(const std::string& cookie)
{
    if (cookie.empty()) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lg(_main_mutex);
        if (_is_processing) {
            return false;
        }

        this->_http_cookie = cookie;
    }

    return true;
}

void NetChannel::enable_callback(NetResultType nrt)
{
    {
        std::lock_guard<std::mutex> lg(_main_mutex);
        if (_is_processing) {
            return;
        }

        if (auto flag = CHECK_NET_RESULT_TYPE(nrt, NetResultType::NRT_ONCB_HEADER)) {
            this->_callback_switches |= flag;
        }

        if (auto flag = CHECK_NET_RESULT_TYPE(nrt, NetResultType::NRT_ONCB_PROGRESS)) {
            this->_callback_switches |= flag;
        }

        if (auto flag = CHECK_NET_RESULT_TYPE(nrt, NetResultType::NRT_ONCB_WRITE)) {
            this->_callback_switches |= flag;
        }

        if (auto flag = CHECK_NET_RESULT_TYPE(nrt, NetResultType::NRT_ONCB_FINISH)) {
            this->_callback_switches |= flag;
        }
    }
}

void NetChannel::disable_callback(NetResultType nrt)
{
    {
        std::lock_guard<std::mutex> lg(_main_mutex);
        if (_is_processing) {
            return;
        }

        if (auto flag = CHECK_NET_RESULT_TYPE(nrt, NetResultType::NRT_ONCB_HEADER)) {
            this->_callback_switches ^= flag;
        }

        if (auto flag = CHECK_NET_RESULT_TYPE(nrt, NetResultType::NRT_ONCB_PROGRESS)) {
            this->_callback_switches ^= flag;
        }

        if (auto flag = CHECK_NET_RESULT_TYPE(nrt, NetResultType::NRT_ONCB_WRITE)) {
            this->_callback_switches ^= flag;
        }

        if (auto flag = CHECK_NET_RESULT_TYPE(nrt, NetResultType::NRT_ONCB_FINISH)) {
            this->_callback_switches ^= flag;
        }
    }
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

    CURLcode ret = CURLE_OK;
    CurlMimeHandle cmh;
    {
        std::lock_guard<std::mutex> lg(_main_mutex);
        if (_is_processing) {
            return false;
        }

        if (!this->_http_body.empty()) {
            return false;
        }
        cmh.Set(curl_mime_init(this->_net_handle.Get()));
    }

    if (!cmh.IsValid()) {
        IMMEDIATE_CRASH();
    }

    auto part = curl_mime_addpart(cmh.Get());
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
    if (ret != CURLE_OK) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lg(_main_mutex);
        _mime_part = std::move(cmh);
    }

    return true;
}

bool NetChannel::set_mime_file(
    const std::string& part_name,
    const std::string& file_path, const std::string& remote_name,
    const std::string& part_type)
{
    if (part_name.empty() || file_path.empty() || remote_name.empty()) {
        return false;
    }

    CURLcode ret = CURLE_OK;
    CurlMimeHandle cmh;
    {
        std::lock_guard<std::mutex> lg(_main_mutex);
        if (_is_processing) {
            return false;
        }

        if (!this->_http_body.empty()) {
            return false;
        }

        cmh.Set(curl_mime_init(this->_net_handle.Get()));
    }

    if (!cmh.IsValid()) {
        IMMEDIATE_CRASH();
    }
    auto part = curl_mime_addpart(cmh.Get());
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
    if (ret != CURLE_OK) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lg(_main_mutex);
        _mime_part = std::move(cmh);
    }
    
    return true;
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

        _http_response_progress.startup_time =
            base::Time::Now().ToDeltaSinceWindowsEpoch().InMilliseconds();
        _is_processing = true;
    }

    setup_curl_opts();
    
    _host_service->post_request(std::bind(
        &NetService::on_channel_request, _host_service, this,
        url, callback, param, timeout_ms, std::placeholders::_1));
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

        _http_response_progress.startup_time =
            base::Time::Now().ToDeltaSinceWindowsEpoch().InMilliseconds();
        _is_processing = true;
    }

    setup_curl_opts();

    _host_service->send_request(std::bind(
        &NetService::on_channel_request, _host_service, this,
        url, callback, param, timeout_ms, std::placeholders::_1));
    baselog::trace("[ns] channel send_request done");
    return true;
}

void NetChannel::send_stop()
{
    {
        std::lock_guard<std::mutex> lg(_main_mutex);
        if (!_is_processing) {
            return;
        }
    }

    baselog::trace("[ns] send_stop posting...");
    _host_service->send_request(std::bind(
        &NetService::on_channel_close, this->_host_service, this, std::placeholders::_1));

    reset_spe_thread_safe();
    baselog::trace("[ns] send_stop all done");
}

void NetChannel::post_stop()
{
    {
        std::lock_guard<std::mutex> lg(_main_mutex);
        if (!_is_processing) {
            return;
        }
    }
 
    _host_service->post_request(std::bind(
        &NetService::on_channel_close, this->_host_service, this, std::placeholders::_1));
}

bool NetChannel::is_callback_switches_exist(NetResultType nrt)
{
    std::lock_guard<std::mutex> lg(_main_mutex);
    return IS_NET_RESULT_TYPE_CONTAIN(_callback_switches, nrt);
}

void NetChannel::reset_all_thread_safe()
{
    {
        std::lock_guard<std::mutex> lg(_main_mutex);
        //_is_processing = false;
        _host_service = nullptr;
        //http opts
        _request_headers.Close();
        _mime_part.Close();
        _http_body.clear();
        _http_cookie.clear();

        _callback_switches = static_cast<std::uint32_t>(
            NetResultType::NRT_ONCB_FINISH);

        //time
        _finish_time_ms = 0;
        //response stuff
        _http_response_header.clear();
        _http_response_content.clear();
        _http_response_progress = NetResultProgress();
        _http_response_finish = NetResultFinish();
    }
}

void NetChannel::reset_spe_thread_safe()
{
    {
        std::lock_guard<std::mutex> lg(_main_mutex);
        _is_processing = false;
    }
    curl_easy_reset(this->_net_handle.Get());
}

void NetChannel::feed_http_response_header(const char* buf, std::size_t len)
{
    std::lock_guard<std::mutex> lg(_main_mutex);
    if ((_http_response_header.size() + len) > kMaxHttpResponseBufSize) {
        return;
    }

    this->_http_response_header.append(buf, len);
}

void NetChannel::feed_http_response_content(const char* buf, std::size_t len)
{
    std::lock_guard<std::mutex> lg(_main_mutex);
    if ((_http_response_content.size() + len) > kMaxHttpResponseBufSize) {
        return;
    }
    this->_http_response_content.append(buf, len);
}

bool NetChannel::feed_http_response_progress(std::int64_t dltotal,
    std::int64_t dlnow, std::int64_t ultotal, std::int64_t ulnow)
{
    if (dltotal == 0 && dlnow == 0 && ultotal == 0 && ulnow == 0) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lg(_main_mutex);
        if (dltotal != _http_response_progress.download_total_size ||
            dlnow != _http_response_progress.download_transfered_size ||
            ultotal != _http_response_progress.upload_total_size ||
            ulnow != _http_response_progress.upload_transfered_size) {
            _http_response_progress.download_total_size = dltotal;
            _http_response_progress.download_transfered_size = dlnow;
            _http_response_progress.upload_total_size = ultotal;
            _http_response_progress.upload_transfered_size = ulnow;
            _http_response_progress.current_time = base::Time::Now().ToDeltaSinceWindowsEpoch().InMilliseconds();
            _http_response_progress.download_speed = (static_cast<std::double_t>(
                _http_response_progress.download_transfered_size) * 1000.0 /
                (_http_response_progress.current_time - _http_response_progress.startup_time));
            _http_response_progress.upload_speed = (static_cast<std::double_t>(
                _http_response_progress.upload_transfered_size) * 1000.0 /
                (_http_response_progress.current_time - _http_response_progress.startup_time));
            return true;
        }
    }

    return false;
}

void NetChannel::get_http_response_progress(NetResultProgress& np)
{
    std::lock_guard<std::mutex> lg(_main_mutex);
    np = _http_response_progress;
}

void NetChannel::get_http_response_finish(NetResultFinish& nrf)
{
    //get metrics from libcurl
    /*
    Times
    An overview of the six time values available from curl_easy_getinfo()

    curl_easy_perform()
    |
    |--NAMELOOKUP
    |--|--CONNECT
    |--|--|--APPCONNECT
    |--|--|--|--PRETRANSFER
    |--|--|--|--|--STARTTRANSFER
    |--|--|--|--|--|--TOTAL
    |--|--|--|--|--|--REDIRECT
    NAMELOOKUP

    CURLINFO_NAMELOOKUP_TIME and CURLINFO_NAMELOOKUP_TIME_T. The time it took from the start until the name resolving was completed.

    CONNECT

    CURLINFO_CONNECT_TIME and CURLINFO_CONNECT_TIME_T. The time it took from the start until the connect to the remote host (or proxy) was completed.

    APPCONNECT

    CURLINFO_APPCONNECT_TIME and CURLINFO_APPCONNECT_TIME_T. The time it took from the start until the SSL connect/handshake with the remote host was completed. (Added in 7.19.0) The latter is the integer version (measuring microseconds). (Added in 7.60.0)

    PRETRANSFER

    CURLINFO_PRETRANSFER_TIME and CURLINFO_PRETRANSFER_TIME_T. The time it took from the start until the file transfer is just about to begin. This includes all pre-transfer commands and negotiations that are specific to the particular protocol(s) involved.

    STARTTRANSFER

    CURLINFO_STARTTRANSFER_TIME and CURLINFO_STARTTRANSFER_TIME_T. The time it took from the start until the first byte is received by libcurl.

    TOTAL

    CURLINFO_TOTAL_TIME and CURLINFO_TOTAL_TIME_T. Total time of the previous request.

    REDIRECT

    CURLINFO_REDIRECT_TIME and CURLINFO_REDIRECT_TIME_T. The time it took for all redirection steps include name lookup, connect, pretransfer and transfer before final transaction was started. So, this is zero if no redirection took place.
    */

    //http response code
    auto ret = curl_easy_getinfo(get_handle(), CURLINFO_RESPONSE_CODE, &nrf.http_response_code);
    //http version
    curl_easy_getinfo(get_handle(), CURLINFO_HTTP_VERSION, &nrf.http_version);
    //total time
    curl_easy_getinfo(get_handle(), CURLINFO_TOTAL_TIME_T, &nrf.total_time_ms);
    nrf.total_time_ms /= 1000;
    //namelookup time
    curl_easy_getinfo(get_handle(), CURLINFO_NAMELOOKUP_TIME_T, &nrf.namelookup_time_ms);
    nrf.namelookup_time_ms /= 1000;
    //connected time
    curl_easy_getinfo(get_handle(), CURLINFO_CONNECT_TIME_T, &nrf.connected_time_ms);
    nrf.connected_time_ms /= 1000;
    // app connected time
    curl_easy_getinfo(get_handle(), CURLINFO_APPCONNECT_TIME_T, &nrf.app_connected_time_ms);
    nrf.app_connected_time_ms /= 1000;
    // pretransfer time
    curl_easy_getinfo(get_handle(), CURLINFO_PRETRANSFER_TIME_T, &nrf.pretransfer_time_ms);
    nrf.pretransfer_time_ms /= 1000;
    // start transfer time 
    curl_easy_getinfo(get_handle(), CURLINFO_STARTTRANSFER_TIME_T, &nrf.starttransfer_time_ms);
    nrf.starttransfer_time_ms /= 1000;
    // redirect time
    curl_easy_getinfo(get_handle(), CURLINFO_REDIRECT_TIME_T, &nrf.redirect_time_ms);
    nrf.redirect_time_ms /= 1000;
    // redirect count
    curl_easy_getinfo(get_handle(), CURLINFO_REDIRECT_COUNT, &nrf.redirect_count);
    // uploaded size
    curl_easy_getinfo(get_handle(), CURLINFO_SIZE_UPLOAD_T, &nrf.uploaded_size_bytes);
    // downloaded size
    curl_easy_getinfo(get_handle(), CURLINFO_SIZE_DOWNLOAD_T, &nrf.playload_size_bytes);
    // download speed
    curl_easy_getinfo(get_handle(), CURLINFO_SPEED_DOWNLOAD_T, &nrf.download_speed_bytes_persecond);
    // upload speed
    curl_easy_getinfo(get_handle(), CURLINFO_SPEED_UPLOAD_T, &nrf.upload_speed_bytes_persecond);
    //content length upload
    curl_easy_getinfo(get_handle(), CURLINFO_CONTENT_LENGTH_UPLOAD_T, &nrf.content_length_upload);
    //content length download (from Content-Length: )
    curl_easy_getinfo(get_handle(), CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &nrf.content_length_download);
    //content type
    curl_easy_getinfo(get_handle(), CURLINFO_CONTENT_TYPE, &nrf.http_content_type);
    //client ip && port string
    curl_easy_getinfo(get_handle(), CURLINFO_PRIMARY_IP, &nrf.primary_ip_string);
    curl_easy_getinfo(get_handle(), CURLINFO_PRIMARY_PORT, &nrf.primary_port);
    //local ip && port string
    curl_easy_getinfo(get_handle(), CURLINFO_LOCAL_IP, &nrf.local_ip_string);
    curl_easy_getinfo(get_handle(), CURLINFO_LOCAL_PORT, &nrf.local_port);
    //scheme type
    curl_easy_getinfo(get_handle(), CURLINFO_SCHEME, &nrf.scheme_type);
    //referrer header
    ret = curl_easy_getinfo(get_handle(), CURLINFO_REFERER, &nrf.referrer_header);
    //last effective url && method
    curl_easy_getinfo(get_handle(), CURLINFO_EFFECTIVE_URL, &nrf.last_effective_url);
    curl_easy_getinfo(get_handle(), CURLINFO_EFFECTIVE_METHOD, &nrf.last_effective_method);
    
    std::lock_guard<std::mutex> lg(_main_mutex);
    nrf.data = this->_http_response_content.c_str();
    nrf.data_len = this->_http_response_content.size();
    nrf.app_average_speed = this->_http_response_progress.download_speed;
    nrf.http_header = this->_http_response_header.c_str();
    nrf.http_header_len = this->_http_response_header.size();
    nrf.result_code = this->_http_response_finish.result_code;
    _http_response_finish = nrf;
}

void NetChannel::feed_http_result_code(NetResultCode code)
{
    std::lock_guard<std::mutex> lg(_main_mutex);
    this->_http_response_finish.result_code = code;
}

void NetChannel::feed_http_finish_time_ms()
{
    auto time = base::Time::Now().ToDeltaSinceWindowsEpoch().InMilliseconds();
    std::lock_guard<std::mutex> lg(_main_mutex);
    this->_finish_time_ms = time;
}

} //namespace netcore

