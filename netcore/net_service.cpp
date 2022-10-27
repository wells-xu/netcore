#include "stdafx.h"
#include "net_service.h"

#include "net_channel.h"

#include <base/log/logger.h>
#include <base/comm/immediate_crash.h>
#include <base/comm/valid_enum.h>

REGISTER_ENUM_VALUES(netcore::NetResultCode,
    netcore::NetResultCode::CURLE_OK,
    netcore::NetResultCode::CURLE_UNSUPPORTED_PROTOCOL,
    netcore::NetResultCode::CURLE_FAILED_INIT,
    netcore::NetResultCode::CURLE_URL_MALFORMAT,
    netcore::NetResultCode::CURLE_NOT_BUILT_IN,
    netcore::NetResultCode::CURLE_COULDNT_RESOLVE_PROXY,
    netcore::NetResultCode::CURLE_COULDNT_RESOLVE_HOST,
    netcore::NetResultCode::CURLE_COULDNT_CONNECT,
    netcore::NetResultCode::CURLE_REMOTE_ACCESS_DENIED,
    netcore::NetResultCode::CURLE_HTTP2,
    netcore::NetResultCode::CURLE_PARTIAL_FILE,
    netcore::NetResultCode::CURLE_WRITE_ERROR,
    netcore::NetResultCode::CURLE_UPLOAD_FAILED,
    netcore::NetResultCode::CURLE_READ_ERROR,
    netcore::NetResultCode::CURLE_OUT_OF_MEMORY,
    netcore::NetResultCode::CURLE_OPERATION_TIMEDOUT,
    netcore::NetResultCode::CURLE_RANGE_ERROR,
    netcore::NetResultCode::CURLE_SSL_CONNECT_ERROR,
    netcore::NetResultCode::CURLE_BAD_DOWNLOAD_RESUME,
    netcore::NetResultCode::CURLE_TOO_MANY_REDIRECTS,
    netcore::NetResultCode::CURLE_UNKNOWN_OPTION,
    netcore::NetResultCode::CURLE_BAD_CONTENT_ENCODING,
    netcore::NetResultCode::CURLE_PROXY,
    netcore::NetResultCode::CURLE_UNKOWN_ERROR);

namespace netcore {

HandleShell::HandleShell() :
    _handle(::CreateEvent(NULL, TRUE, FALSE, NULL))
{
}
HandleShell::~HandleShell() = default;

NetService::NetService()
{
}

NetService::~NetService()
{
    baselog::trace("[ns] ~NetService");
}

bool NetService::init()
{
    {
		std::unique_lock<std::mutex> ul(_main_mutex);
        if (!_is_stopped) {
            return true;
        }

        if (_net_handle.IsValid()) {
            return false;
        }
        _net_handle.Set(curl_multi_init());

        if (!_channel_pool.init()) {
            baselog::error("[ns] channel pool inited failed");
            return false;
        }

        if (!_event_pool.init()) {
            baselog::error("[ns] event pool inited failed");
            return false;
        }

        if (!_curl_private_pool.init()) {
            baselog::error("[ns] private pool inited failed");
            return false;
        }

        if (!_short_buffer_pool.init(16, 10240)) {
            baselog::error("[ns] short buffer pool inited failed");
            return false;
        }

        if (!_wrote_buffer_pool.init(16, 10240)) {
            baselog::error("[ns] short buffer pool inited failed");
            return false;
        }

        _thread = std::move(std::thread(&NetService::thread_proc, this));
        _user_thread = std::move(std::thread(&NetService::user_thread_proc, this));
        _is_stopped = false;
    }

    baselog::info("net service started now...");
    return true;
}

bool NetService::close()
{
    baselog::info("netcore service is closing...");
    {
        std::unique_lock<std::mutex> ul(_main_mutex);
        if (_is_stopped) {
            return false;
        }

        _is_stopped = true;
    }
    
    wake_up_event();

    if (_thread.joinable()) {
        _thread.join();
    }

    if (_user_thread.joinable()) {
        _user_thread.join();
    }

    _net_handle.Close();
    return true;
}

void NetService::wake_up_event()
{
    _user_loop_event.notify_one();
    curl_multi_wakeup(this->_net_handle.Get());
}

void NetService::do_pending_task(std::deque<TaskInfo>& tasks)
{
    while(!tasks.empty()) {
        auto &cb = tasks.front();
        cb.cb();
        tasks.pop_front();
    }
}

void NetService::do_finish_channel()
{
    //read completed channels
    do {
        int msgq = 0;
        auto msg = curl_multi_info_read(_net_handle.Get(), &msgq);
        if (msg == nullptr) {
             baselog::trace("[ns] do_finish_channel_threadin empty");
             break;
        }

        baselog::trace("[ns] do_finish_channel new channel done: {}, errcode= {}",
            (void*)msg->easy_handle, (int)msg->data.result);

        LibcurlPrivateInfo* ptr_pri = nullptr;
        curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &ptr_pri);
        if (ptr_pri == nullptr) {
            IMMEDIATE_CRASH();
        }
        if (ptr_pri->chan == nullptr) {
            IMMEDIATE_CRASH();
        }
        NetResultCode nrc = static_cast<NetResultCode>(CURLE_QUOTE_ERROR);
        
        if (ptr_pri->chan->is_callback_switches_exist(NetResultType::NRT_ONCB_FINISH)) {
            auto ret = curl_multi_remove_handle(_net_handle.Get(), ptr_pri->chan->get_handle());
            if (ret != CURLM_OK) {
                IMMEDIATE_CRASH();
                return;
            }

            this->add_user_callback(UserCallbackTask
                { nullptr, NetResultType::NRT_ONCB_FINISH, ptr_pri });
        } else {
            this->on_channel_close(ptr_pri->chan);
            this->restore_private_info(ptr_pri);
        }
     } while (1);
}

void NetService::thread_proc()
{
    do {
        int still_running = 0;
        auto mc = curl_multi_perform(_net_handle.Get(), &still_running);
        if (mc != CURLM_OK) {
            baselog::error("[ns] curl_multi_perform failed");
            break;
        }

        std::deque<TaskInfo> cache_tasks;
        {
            std::unique_lock<std::mutex> ul(_main_mutex);
            if (_is_stopped) {
                baselog::warn("[ns] main work thread exited, pending task num= {}", _pending_tasks.size());
                do_pending_task(_pending_tasks);
                break;
            }

            if (!_pending_tasks.empty()) {
                cache_tasks.swap(_pending_tasks);
            }
        }

        //process pending tasks
        do_pending_task(cache_tasks);
        //process finished channels
        do_finish_channel();

        int numfds = 0;
        mc = curl_multi_poll(_net_handle.Get(), NULL, 0, 5000, &numfds);
        if (mc != CURLM_OK) {
            baselog::error("[ns] curl_multi_poll failed");
            break;
        }
    } while (1);
}

void NetService::user_thread_proc()
{
    do {
        std::deque<UserCallbackTask> user_cbs;
        {
            std::unique_lock<std::mutex> ul(_user_mutex);
            _user_loop_event.wait(ul, [this]() {
                return (_is_stopped || !_pending_user_callbacks.empty());
            });

            if (_is_stopped) {
                baselog::warn("[ns] user work thread exited, pending task num= {}", _pending_user_callbacks.size());
                //while (!_pending_user_callbacks.empty()) {
                //    _pending_user_callbacks.front()->reset(); 
                //    restore_private_info(_pending_user_callbacks.front());
                //    _pending_user_callbacks.pop_front();
                //}
                _pending_user_callbacks.clear();
                break;
            }

            baselog::trace("[ns] user_thread_proc event fired...size= {}", _pending_user_callbacks.size());
            if (!_pending_user_callbacks.empty()) {
                user_cbs.swap(_pending_user_callbacks);
            }
        }

        do_user_pending_tasks(user_cbs);
    } while (1);
}

void NetService::do_user_pending_tasks(std::deque<UserCallbackTask>& tasks)
{
    baselog::trace("[ns] do_user_pending_tasks total size= {}", tasks.size());
    while (!tasks.empty()) {
        auto &task = tasks.front();
       
        baselog::trace("[ns] do_user_pending_tasks type= {}", (int)task.delivered_type);
        if (task.pri->cb) {
            if (task.delivered_type == NetResultType::NRT_ONCB_HEADER) {
                NetResultHeader nrh;
                nrh.content = task.data->c_str();
                nrh.content_len = task.data->size();

                task.pri->cb(task.delivered_type,
                    reinterpret_cast<void*>(&nrh), task.pri->param);

                if (!restore_short_buffer(task.data)) {
                    IMMEDIATE_CRASH();
                }
            } else if (task.delivered_type == NetResultType::NRT_ONCB_PROGRESS) {
                NetResultProgress nrp;
                task.pri->chan->get_http_response_progress(nrp);

                task.pri->cb(task.delivered_type,
                    reinterpret_cast<void*>(&nrp), task.pri->param);
            } else if (task.delivered_type == NetResultType::NRT_ONCB_WRITE) {
                NetResultWrite nrw;
                nrw.content = task.data->c_str();
                nrw.content_len = task.data->size();

                task.pri->cb(task.delivered_type,
                    reinterpret_cast<void*>(&nrw), task.pri->param);

                if (!restore_wrote_buffer(task.data)) {
                    IMMEDIATE_CRASH();
                }
            } else if (task.delivered_type == NetResultType::NRT_ONCB_FINISH) {
                NetResultFinish nrf;
                task.pri->cb(task.delivered_type,
                    reinterpret_cast<void*>(&nrf), task.pri->param);

                clean_channel(task.pri->chan);
                if (!restore_private_info(task.pri)) {
                    IMMEDIATE_CRASH();
                }
            }
        }

        tasks.pop_front();
    }
}

INetChannel* NetService::create_channel()
{
    {
        std::unique_lock<std::mutex> ul(_main_mutex);
        if (_is_stopped) {
            return false;
        }
    }

    auto chan = this->_channel_pool.borrow();
    if (chan == nullptr) {
        return nullptr;
    }

    if (!chan->init(this)) {
        baselog::fatal("create channel failed");
        return nullptr;
    }
    baselog::info("[netcore] create_channel successed: {}", (void*)chan);
    return chan;
}

INetChannel* NetService::create_clone_channel(INetChannel* chan)
{
    if (chan == nullptr) {
        IMMEDIATE_CRASH();
    }

    {
        std::unique_lock<std::mutex> ul(_main_mutex);
        if (_is_stopped) {
            return false;
        }
    }

    if (!_channel_pool.is_valid(dynamic_cast<NetChannel*>(chan))) {
        return nullptr;
    }

    auto new_chan = this->_channel_pool.borrow();
    if (new_chan == nullptr) {
        IMMEDIATE_CRASH();
        return nullptr;
    }

    if (!new_chan->init(this, dynamic_cast<NetChannel*>(chan))) {
        //baselog::fatal("create channel failed");
        IMMEDIATE_CRASH();
        return nullptr;
    }

    baselog::info("[netcore] create_clone_channel successed: {}", (void*)new_chan);
    return new_chan;
}

void NetService::remove_channel(INetChannel* chan)
{
    baselog::info("[ns] remove_channel: {}", (void*)chan);
    NetChannel *channel = dynamic_cast<NetChannel*>(chan);
    if (channel == nullptr) {
        return;
    }

    this->post_request(std::bind(
        &NetService::on_channel_remove, this, channel));
}

void NetService::send_request(NSCallBack callback)
{
    post_request(callback);

    auto sync_handle = _event_pool.borrow();
    if (sync_handle == nullptr) {
        IMMEDIATE_CRASH();
        return;
    }
}

void NetService::post_request(NSCallBack callback)
{
    {
        std::unique_lock<std::mutex> ul(_main_mutex);
        if (_is_stopped) {
            return;
        }
    }

    TaskInfo ti{ callback };
    {
        std::unique_lock<std::mutex> ul(_main_mutex);
        _pending_tasks.push_back(ti);
    }

    this->wake_up_event();
}

HandleShell* NetService::borrow_event_shell()
{
    auto phs = _event_pool.borrow();
    if (phs == nullptr) {
        IMMEDIATE_CRASH();
    }

    return phs;
}

bool NetService::restore_event_shell(HandleShell* hs)
{
    if (hs == nullptr) {
        IMMEDIATE_CRASH();
    }

    ::ResetEvent(hs->Get());
    return _event_pool.retrieve(hs);
}

LibcurlPrivateInfo* NetService::borrow_private_info()
{
    auto ptr = _curl_private_pool.borrow();
    if (ptr == nullptr) {
        IMMEDIATE_CRASH();
    }

    return ptr;
}

bool NetService::restore_private_info(LibcurlPrivateInfo* ptr)
{
    if (ptr == nullptr) {
        IMMEDIATE_CRASH();
    }
    ptr->reset();
    return _curl_private_pool.retrieve(ptr);
}

std::string* NetService::borrow_short_buffer()
{
    auto ptr = _short_buffer_pool.borrow();
    if (ptr == nullptr) {
        IMMEDIATE_CRASH();
    }

    return ptr;
}

bool NetService::restore_short_buffer(std::string* ptr)
{
    if (ptr == nullptr) {
        IMMEDIATE_CRASH();
    }
    ptr->clear();
    return _short_buffer_pool.retrieve(ptr);
}

std::string* NetService::borrow_wrote_buffer()
{
    auto ptr = _wrote_buffer_pool.borrow();
    if (ptr == nullptr) {
        IMMEDIATE_CRASH();
    }

    return ptr;
}

bool NetService::restore_wrote_buffer(std::string* ptr)
{
    if (ptr == nullptr) {
        IMMEDIATE_CRASH();
    }
    ptr->clear();
    return _wrote_buffer_pool.retrieve(ptr);
}

bool NetService::on_channel_close(NetChannel* channel)
{
    if (channel == nullptr) {
        IMMEDIATE_CRASH();
        return false;
    }

    baselog::trace("[ns] on_channel_close remove channel from libcurl...");
    auto ret = curl_multi_remove_handle(_net_handle.Get(), channel->get_handle());
    if (ret != CURLM_OK) {
        IMMEDIATE_CRASH();
        return false;
    }

    clean_channel(channel);
    return true;
}

void NetService::clean_channel(NetChannel* channel)
{
    auto event = channel->get_wait_event();
    if (event != nullptr) {
        ::SetEvent(event->Get());
    } else {
        channel->reset_thread_safe();
    }
}

bool NetService::on_channel_remove(NetChannel* channel)
{
    baselog::trace("[ns] on_channel_remove chan= {}", (void*)channel);
    this->on_channel_close(channel);
    if (!this->_channel_pool.retrieve(channel)) {
        IMMEDIATE_CRASH();
        return false;
    }

    return true;
}

size_t NetService::on_callback_curl_write(
    char* buffer, size_t size, size_t nmemb, void* userdata)
{
    auto ptr_pri = reinterpret_cast<LibcurlPrivateInfo*>(userdata);
    if (ptr_pri == nullptr) {
        IMMEDIATE_CRASH();
    }
    if (ptr_pri->chan == nullptr) {
        IMMEDIATE_CRASH();
    }
    //curl_off_t  content_len = 0;
    //auto ret = curl_easy_getinfo(chan->chan->get_handle(), CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &content_len);
    ptr_pri->chan->feed_http_response_content(buffer, (size * nmemb));
    if (ptr_pri->chan->is_callback_switches_exist(NetResultType::NRT_ONCB_WRITE)) {
        auto buf = ptr_pri->chan->host_service()->borrow_wrote_buffer();
        if (buf == nullptr) {
            IMMEDIATE_CRASH();
        }
        buf->assign(buffer, size * nmemb);
        ptr_pri->chan->host_service()->add_user_callback(
            UserCallbackTask{ buf, NetResultType::NRT_ONCB_WRITE, ptr_pri });
    }

    baselog::trace("[ns] on_callback_curl_write received size= {}", size * nmemb);
    return size_t(size * nmemb);
}

void NetService::add_user_callback(UserCallbackTask task)
{
    {
        std::unique_lock<std::mutex> ul(_user_mutex);
        _pending_user_callbacks.push_back(task);
    }
    baselog::trace("[ns] add_user_callback event emited...");
    _user_loop_event.notify_one();
}

size_t NetService::on_callback_curl_head(
    char* buffer, size_t size, size_t nitems, void* userdata)
{
    auto ptr_pri = reinterpret_cast<LibcurlPrivateInfo*>(userdata);
    if (ptr_pri == nullptr) {
        IMMEDIATE_CRASH();
    }
    baselog::trace("[ns] on_callback_curl_head: {}", std::string(buffer, size * nitems));

    if (ptr_pri->chan == nullptr) {
        IMMEDIATE_CRASH();
    }

    ptr_pri->chan->feed_http_response_header(buffer, (size * nitems));
    if (ptr_pri->chan->is_callback_switches_exist(NetResultType::NRT_ONCB_HEADER)) {
        auto buf = ptr_pri->chan->host_service()->borrow_short_buffer();
        if (buf == nullptr) {
            IMMEDIATE_CRASH();
        }
        buf->assign(buffer, size * nitems);
        ptr_pri->chan->host_service()->add_user_callback(
            UserCallbackTask{buf, NetResultType::NRT_ONCB_HEADER, ptr_pri});
    }
    return size_t(size * nitems);
}

int NetService::on_callback_curl_progress(
    void* clientp,
    curl_off_t dltotal, curl_off_t dlnow,
    curl_off_t ultotal, curl_off_t ulnow)
{
    auto ptr_pri = reinterpret_cast<LibcurlPrivateInfo*>(clientp);
    baselog::trace("[ns] on_callback_curl_progress info= ({}.{}.{}.{})-{}",
        dltotal, dlnow, ultotal, ulnow, (void*)ptr_pri->chan);

    if (ptr_pri == nullptr) {
        IMMEDIATE_CRASH();
    }

    if (ptr_pri->chan == nullptr) {
        IMMEDIATE_CRASH();
    }

    if (!ptr_pri->chan->feed_http_response_progress(dltotal, dlnow, ultotal, ulnow)) {
        return 0;
    }

    if (ptr_pri->chan->is_callback_switches_exist(NetResultType::NRT_ONCB_PROGRESS)) {
        ptr_pri->chan->host_service()->add_user_callback(
            UserCallbackTask{ nullptr, NetResultType::NRT_ONCB_PROGRESS, ptr_pri });
    }
    return 0;
}

bool NetService::on_channel_request(NetChannel* chan,
    const std::string url, CallbackType cb, void* param, int timeout_ms)
{
    if (chan == nullptr) {
        IMMEDIATE_CRASH();
    }

    //create && assign private data
    auto pri_ptr = reinterpret_cast<void*>(borrow_private_info());
    baselog::trace("[ns] new private info created: {}", pri_ptr);
    auto lpi = reinterpret_cast<LibcurlPrivateInfo*>(pri_ptr);
    lpi->chan = chan;
    lpi->cb = cb;
    lpi->param = param;
    lpi->url = url;
    lpi->timeout_ms = timeout_ms;

    //libcurl set opts

    //proxy stuff upport
    //curl_easy_setopt(curl, CURLOPT_PROXY, @proxy_);
    //curl_easy_setopt(curl, CURLOPT_PROXYUSERNAME, @user_name_);
    //curl_easy_setopt(curl, CURLOPT_PROXYPASSWORD, @user_pass_);

    //http/https curl
    curl_easy_setopt(chan->get_handle(), CURLOPT_URL, url.c_str());

    //enable all supported built-in compressions
    curl_easy_setopt(chan->get_handle(), CURLOPT_ACCEPT_ENCODING, "");

    //redirect setting:CURL_REDIR_POST_ALL = (CURL_REDIR_POST_301 | CURL_REDIR_POST_302 |CURL_REDIR_POST_303)
    curl_easy_setopt(chan->get_handle(), CURLOPT_POSTREDIR, CURL_REDIR_POST_ALL);
    curl_easy_setopt(chan->get_handle(), CURLOPT_FOLLOWLOCATION, 1L);

    //https/ssl/tls stuff
    //xp support
    //if (IsWInXP()) {
        //curl_easy_setopt(chan->get_handle(), CURLOPT_SSLVERSION, CURL_SSLVERSION_SSLv3);
    //}
    //This option determines whether curl verifies the authenticity of the peer's certificate.
    //By default, curl assumes a value of 1.
    //curl_easy_setopt(chan->get_handle(), CURLOPT_SSL_VERIFYPEER, 1L);
    //This option determines whether libcurl verifies that the server cert is for the server it is known as.
    //When the verify value is 0, the connection succeeds regardless of the names in the certificate. Use that ability with caution!
    //The default value for this option is 2.
    //curl_easy_setopt(chan->get_handle(), CURLOPT_SSL_VERIFYHOST, 2);

    //timeout stuff
    //the maximum time in milliseconds that you allow the libcurl transfer operation to take.
    //Default timeout is 0 (zero) which means it never times out during transfer.
    curl_easy_setopt(chan->get_handle(), CURLOPT_TIMEOUT_MS, timeout_ms);
    //Set to zero to switch to the default built-in connection timeout - 300 seconds
    //curl_easy_setopt(chan->get_handle(), CURLOPT_CONNECTTIMEOUT, (timeout_ms / 1000) + 1);
    //curl_easy_setopt(chan->get_handle(), CURLOPT_CONNECTTIMEOUT, (timeout_ms / 1000) + 10);
    //curl_easy_setopt(chan->get_handle(), CURLOPT_CONNECTTIMEOUT_MS, timeout_ms); 

    //private setting
    curl_easy_setopt(chan->get_handle(), CURLOPT_PRIVATE, pri_ptr);

    //write
    curl_easy_setopt(chan->get_handle(), CURLOPT_WRITEDATA, pri_ptr);
    curl_easy_setopt(chan->get_handle(),
        CURLOPT_WRITEFUNCTION, &NetService::on_callback_curl_write);
    //progress
    curl_easy_setopt(chan->get_handle(), CURLOPT_XFERINFODATA, pri_ptr);
    curl_easy_setopt(chan->get_handle(),
        CURLOPT_XFERINFOFUNCTION, &NetService::on_callback_curl_progress);
    curl_easy_setopt(chan->get_handle(), CURLOPT_NOPROGRESS, 0);

    //socket (not used)
    //curl_easy_setopt(chan->get_handle(), CURLOPT_SOCKOPTFUNCTION, &CNetManager::curl_after_sock_create_cb);
    //curl_easy_setopt(chan->get_handle(), CURLOPT_SOCKOPTDATA, pri_ptr);

    //head
    curl_easy_setopt(chan->get_handle(),
        CURLOPT_HEADERFUNCTION, &NetService::on_callback_curl_head);
    curl_easy_setopt(chan->get_handle(), CURLOPT_HEADERDATA, pri_ptr);

    auto ret = curl_multi_add_handle(this->_net_handle.Get(), chan->get_handle());
    if (ret != CURLE_OK) {
        baselog::warn("[ns] curl_multi_add_handle failed");
    }
    baselog::trace("[ns] on_channel_request new request curl handle added= {}", (void*)chan->get_handle());
    return true;
}

}

