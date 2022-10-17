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

void NetService::do_pending_task_threadin(std::deque<TaskInfo>& tasks)
{
    while(!tasks.empty()) {
        auto &cb = tasks.front();
        cb.cb();
        tasks.pop_front();
    }
}

void NetService::do_finish_channel_threadin()
{
    //read completed channels
    do {
         int msgq = 0;
         auto m = curl_multi_info_read(_net_handle, &msgq);
         if (m == nullptr) {
             baselog::trace("[ns] do_finish_channel_threadin empty");
             break;
         }

         auto chan = m->easy_handle;
         baselog::trace("[ns] do_finish_channel_threadin new channel done: {}", (void*)chan);
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
        do_pending_task_threadin(cache_tasks);
        //process finished channels
        do_finish_channel_threadin();

        int numfds = 0;
        mc = curl_multi_poll(_net_handle, NULL, 0, 5000, &numfds);
        if (mc != CURLM_OK) {
            baselog::error("[ns] curl_multi_poll failed");
            break;
        }
    } while (1);

    //do {
    //    std::deque<TaskInfo> cache_tasks;
    //    {
    //        std::unique_lock<std::mutex> ul(_main_mutex);
    //        _main_loop_event.wait(ul, [this]() {
    //            return (_is_stopped || !_pending_tasks.empty());
    //            });

    //        if (_is_stopped) {
    //            baselog::warn("[ns] main work proc exited, pending task num= {}", _pending_tasks.size());
    //            break;
    //        }
    //        cache_tasks.swap(_pending_tasks);
    //    }

    //    //process pending tasks
    //    do_pending_task_threadin(cache_tasks);

    //    int still_running = 0;
    //    auto mc = curl_multi_perform(_net_handle, &still_running);
    //    if (mc != CURLM_OK) {
    //        baselog::error("[ns] curl_multi_perform failed");
    //        break;
    //    }

    //    int numfds = 0;
    //    mc = curl_multi_poll(_net_handle, NULL, 0, 1000, &numfds);
    //    if (mc != CURLM_OK) {
    //        baselog::error("[ns] curl_multi_poll failed");
    //        break;
    //    }

    //    //read completed channels
    //    do {
    //        int msgq = 0;
    //        auto m = curl_multi_info_read(_net_handle, &msgq);
    //        if (m == nullptr) {
    //            break;
    //        }

    //        auto chan = m->easy_handle;
    //        //curl_multi_remove_handle(_net_handle, m->easy_handle);

    //        //WriteDataMemory* ptr_wdm = nullptr;
    //        //curl_easy_getinfo(chan, CURLINFO_PRIVATE, &ptr_wdm);
    //        //if (ptr_wdm != nullptr) {
    //        //    baselog::info("[ns] channel done: size= {}", ptr_wdm->buf.size());
    //        //    ptr_wdm->buf.clear();
    //        //}
    //        //PrivateBufThreadSafe().retrieve(ptr_wdm);
    //    } while (1);
    //} while (1);
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

    chan->init();
    chan->set_host_service(this);
    baselog::info("[netcore] create_channel: {}", (void*)chan);
    return chan;
}

void NetService::remove_channel(INetChannel* chan)
{
    baselog::info("[ns] remove_channel: {}", (void*)chan);
    NetChannel *channel = dynamic_cast<NetChannel*>(chan);
    if (channel == nullptr) {
        return;
    }

    this->post_request(std::bind(&NetService::on_close_channel, this,
        reinterpret_cast<void*>(channel)));
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

bool NetService::on_close_channel(void* param)
{
    if (param == nullptr) {
        return false;
    }

    NetChannel* channel = reinterpret_cast<NetChannel*>(param);
    auto ret = curl_multi_remove_handle(_net_handle, channel->get_handle());
    if (ret == CURLM_OK) {
        channel->reset();
        return this->_channel_pool.retrieve(channel);
    }

    IMMEDIATE_CRASH();
    return false;
}

size_t NetService::on_callback_curl_write(
    char* ptr, size_t size, size_t nmemb, void* userdata)
{
    baselog::trace("[ns] on_callback_curl_write received size= {}", size * nmemb);
    return size_t(size * nmemb);
}

bool NetService::on_channel_request(NetChannel* chan)
{
    if (chan == nullptr) {
        IMMEDIATE_CRASH();
    }

    auto ret = curl_multi_add_handle(this->_net_handle, chan->get_handle());
    if (ret != CURLE_OK) {
        //this->wake_up_event();
        baselog::warn("[ns] curl_multi_add_handle failed");
    }
    baselog::trace("[ns] curl_multi_add_handle successed with channel= {}", (void*)chan->get_handle());

    void* pri_ptr = (void*)new int;
    curl_easy_setopt(chan->get_handle(), CURLOPT_PRIVATE, pri_ptr);
    curl_easy_setopt(chan->get_handle(), CURLOPT_WRITEDATA, pri_ptr);
    curl_easy_setopt(chan->get_handle(), CURLOPT_WRITEFUNCTION, &NetService::on_callback_curl_write);
    curl_easy_setopt(chan->get_handle(), CURLOPT_XFERINFODATA, pri_ptr);
    //curl_easy_setopt(chan->get_handle(), CURLOPT_XFERINFOFUNCTION, &CNetManager::curl_progress_cb);
    //curl_easy_setopt(chan->get_handle(), CURLOPT_NOPROGRESS, 0);
    //curl_easy_setopt(chan->get_handle(), CURLOPT_SOCKOPTFUNCTION, &CNetManager::curl_after_sock_create_cb);
    //curl_easy_setopt(chan->get_handle(), CURLOPT_SOCKOPTDATA, pri_ptr);
    //curl_easy_setopt(chan->get_handle(), CURLOPT_HEADERFUNCTION, &CNetManager::curl_header_callback);
    //curl_easy_setopt(chan->get_handle(), CURLOPT_HEADERDATA, pri_ptr);
    //curl_easy_setopt(chan, CURLOPT_FOLLOWLOCATION, 1L);
    return true;
}

}

