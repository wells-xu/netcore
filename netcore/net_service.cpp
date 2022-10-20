#include "stdafx.h"
#include "net_service.h"

#include "net_channel.h"

#include <base/log/logger.h>
#include <base/comm/immediate_crash.h>

namespace netcore {

HandleShell::HandleShell() :
    _handle(::CreateEvent(NULL, TRUE, FALSE, NULL))
{
}
HandleShell::~HandleShell() = default;

NetService::NetService() :
    _net_handle(curl_multi_init())
{
}

NetService::~NetService()
{
}

bool NetService::init()
{
    if (_net_handle == nullptr) {
        return false;
    }

    {
		std::unique_lock<std::mutex> ul(_main_mutex);
        if (!_is_stopped) {
            return true;
        }

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

        _thread = std::move(std::thread(&NetService::thread_proc, this));
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
    return (CURLM_OK == curl_multi_cleanup(_net_handle));
}

void NetService::wake_up_event()
{
    _main_loop_event.notify_one();
    curl_multi_wakeup(this->_net_handle);
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
         auto m = curl_multi_info_read(_net_handle, &msgq);
         if (m == nullptr) {
             baselog::trace("[ns] do_finish_channel_threadin empty");
             break;
         }

         baselog::trace("[ns] do_finish_channel_threadin new channel done: {}", (void*)m->easy_handle);
         LibcurlPrivateInfo* ptr_pri = 0;
         curl_easy_getinfo(m->easy_handle, CURLINFO_PRIVATE, &ptr_pri);
         if (ptr_pri == nullptr) {
             IMMEDIATE_CRASH();
         }
         this->on_channel_close(ptr_pri->chan);
         //curl_multi_remove_handle(_net_handle, m->easy_handle);

         //WriteDataMemory* ptr_wdm = nullptr;
         //curl_easy_getinfo(chan, CURLINFO_PRIVATE, &ptr_wdm);
         //if (ptr_wdm != nullptr) {
         //    baselog::info("[ns] channel done: size= {}", ptr_wdm->buf.size());
         //    ptr_wdm->buf.clear();
         //}
         //PrivateBufThreadSafe().retrieve(ptr_wdm);
     } while (1);
}

void NetService::thread_proc()
{
    do {
        int still_running = 0;
        auto mc = curl_multi_perform(_net_handle, &still_running);
        if (mc != CURLM_OK) {
            baselog::error("[ns] curl_multi_perform failed");
            break;
        }

        std::deque<TaskInfo> cache_tasks;
        {
            std::unique_lock<std::mutex> ul(_main_mutex);
            if (_is_stopped) {
                baselog::warn("[ns] main work proc exited, pending task num= {}", _pending_tasks.size());
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
        mc = curl_multi_poll(_net_handle, NULL, 0, 5000, &numfds);
        if (mc != CURLM_OK) {
            baselog::error("[ns] curl_multi_poll failed");
            break;
        }
    } while (1);
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

void NetService::remove_channel(INetChannel* chan)
{
    baselog::info("[ns] remove_channel: {}", (void*)chan);
    NetChannel *channel = dynamic_cast<NetChannel*>(chan);
    if (channel == nullptr) {
        return;
    }

    this->post_request(std::bind(
        &NetService::on_chanel_remove, this, channel));
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
    return _curl_private_pool.retrieve(ptr);
}

bool NetService::on_channel_close(NetChannel* channel)
{
    if (channel == nullptr) {
        IMMEDIATE_CRASH();
        return false;
    }

    baselog::trace("[ns] on_channel_close remove channel from libcurl...");
    auto ret = curl_multi_remove_handle(_net_handle, channel->get_handle());
    if (ret != CURLM_OK) {
        IMMEDIATE_CRASH();
        return false;
    }

    auto event = channel->get_wait_event();
    if (event != nullptr) {
        ::SetEvent(event->Get());
    }
    channel->reset_multi_thread();
    return true;
}

bool NetService::on_chanel_remove(NetChannel* channel)
{
    this->on_channel_close(channel);
    if (!this->_channel_pool.retrieve(channel)) {
        IMMEDIATE_CRASH();
        return false;
    }

    return true;
}

size_t NetService::on_callback_curl_write(
    char* ptr, size_t size, size_t nmemb, void* userdata)
{
    auto chan = reinterpret_cast<LibcurlPrivateInfo*>(userdata);
    curl_off_t  content_len = 0;
    auto ret = curl_easy_getinfo(chan->chan->get_handle(), CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &content_len);
    baselog::trace("[ns] on_callback_curl_write received size= {} content_len= {} content= {}",
        size * nmemb, content_len, std::string(ptr, size * nmemb));
    return size_t(size * nmemb);
}

size_t NetService::on_callback_curl_head(
    char* buffer, size_t size, size_t nitems, void* userdata)
{
    baselog::trace("[ns] on_callback_curl_head content= {}", buffer);
    return size_t(size * nitems);
}

int NetService::on_callback_curl_progress(
    void* clientp,
    curl_off_t dltotal, curl_off_t dlnow,
    curl_off_t ultotal, curl_off_t ulnow)
{
    auto chan = reinterpret_cast<LibcurlPrivateInfo*>(clientp)->chan;
    baselog::trace("[ns] on_callback_curl_progress info= ({}.{}.{}.{})-{}",
        dltotal, dlnow, ultotal, ulnow, (void*)chan);
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
    curl_easy_setopt(chan->get_handle(), CURLOPT_CONNECTTIMEOUT, (timeout_ms / 1000) + 1);
    //curl_easy_setopt(chan->get_handle(), CURLOPT_CONNECTTIMEOUT_MS, timeout_ms);

    auto ret = curl_multi_add_handle(this->_net_handle, chan->get_handle());
    if (ret != CURLE_OK) {
        baselog::warn("[ns] curl_multi_add_handle failed");
    }
    baselog::trace("[ns] on_channel_request new request curl handle added= {}", (void*)chan->get_handle());

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

    //relocation
    //curl_easy_setopt(chan, CURLOPT_FOLLOWLOCATION, 1L);
    return true;
}

}

