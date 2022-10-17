// test_curl.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <chrono>
#include <thread>
#include <iostream>
#include <fstream>

#include <curl\curl.h>

#include <base/comm/distrib_pool.h>
#include <base/comm/no_destructor.h>
#include <base/log/logger.h>

#include <base/strings/string_util.h>
#include <base/strings/utf_string_conversions.h>
#include <base/strings/sys_string_conversions.h>
#include <base/win/scoped_handle.h>

namespace sync_stuff {

struct WriteDataMemory {
    std::string buf;
};

size_t on_write_callback(void *buffer, size_t size, size_t nmemb, void *userp)
{
    WriteDataMemory *ptr = reinterpret_cast<WriteDataMemory*>(userp);
    if (ptr == nullptr) {
        return 0;
    }

    ptr->buf.append(reinterpret_cast<char*>(buffer), size * nmemb);
    return (size * nmemb);
}

int run()
{
    CURL *curl = nullptr;
    CURLcode res;

    WriteDataMemory wdm;
    CURLversion cv = CURLVERSION_FIRST;
    auto vi = curl_version_info(cv);
    curl = curl_easy_init();
    if (curl != nullptr) {
        //curl_easy_setopt(curl, CURLOPT_URL, "https://www.baidu.com");
        curl_easy_setopt(curl, CURLOPT_URL, "https://www.hao123.com");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, on_write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, reinterpret_cast<void*>(&wdm));
        //curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            std::cout << "perform failed: " << curl_easy_strerror(res) << std::endl;
        }
        std::ofstream of("log.content");
        of << wdm.buf.c_str();
        of.close();

        curl_easy_cleanup(curl);
        curl = nullptr;
    }

    return 0;
}

}

namespace async_stuff {

CURLM* g_curl_man;

struct WriteDataMemory {
    std::string buf;
};

size_t on_write_callback(void *buffer, size_t size, size_t nmemb, void *userp)
{
    WriteDataMemory *ptr = reinterpret_cast<WriteDataMemory*>(userp);
    if (ptr == nullptr) {
        return 0;
    }
    auto tid = std::this_thread::get_id();
    //std::cout << "on_write_callback: " << tid << std::endl;

    ptr->buf.append(reinterpret_cast<char*>(buffer), size * nmemb);
    
    //auto str1 = base::CollapseWhitespaceASCII(ptr->buf, true);
    ////std::string str2;
    ////str2.resize(str3.size() * 3);
    ////std::wstring wbuf;
    ////gbk_to_unicode(str3.c_str(), wbuf);
    ////::OutputDebugStringW(wbuf.c_str());
    //auto utf8_str = base::GBKToUTF8(str1);
    //::OutputDebugStringA(utf8_str.c_str());

    baselog::debug("[on_write_cb] net data receieved size= {}", ptr->buf.size());
    //baselog::debug("[on_write_cb] net data receieved content= {}", utf8_str);
    return (size * nmemb);
}

int run()
{
    CURLM *curl_man = nullptr;
    curl_global_init(CURL_GLOBAL_ALL);

    auto tid = std::this_thread::get_id();
    std::cout << "main thread: " << tid << std::endl;

    CURL *curl = nullptr;
    WriteDataMemory wdm;
    curl = curl_easy_init();
    if (curl != nullptr) {
        curl_easy_setopt(curl, CURLOPT_URL, "https://www.baidu.com");
        //curl_easy_setopt(curl, CURLOPT_URL, "https://www.hao123.com");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, on_write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, reinterpret_cast<void*>(&wdm));
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    }

    curl_man = curl_multi_init();
    if (curl == nullptr) {
        return 1;
    }
    auto ret = curl_multi_add_handle(curl_man, curl);
    if (ret != CURLM_OK) {
        curl_easy_cleanup(curl);
        return 2;
    }

    int still_running = 0;
    do {
        int numfds;
        ret = curl_multi_perform(curl_man, &still_running);
        if (ret == CURLM_OK) {
            ret = curl_multi_poll(curl_man, NULL, 0, 10000, &numfds);
        }

        if (ret != CURLM_OK) {
            return 1;
        }
    } while (1);

    return 0;
}

class ScopedCURLHandle {
public:
    ScopedCURLHandle() :
        _curl(curl_easy_init()) {
    }
    ScopedCURLHandle(ScopedCURLHandle&) = delete;
    ScopedCURLHandle(const ScopedCURLHandle&) = delete;
    ScopedCURLHandle(ScopedCURLHandle&& other) = delete;
    ScopedCURLHandle& operator=(const ScopedCURLHandle&) = delete;


    ~ScopedCURLHandle() {
        if (_curl != nullptr) {
            curl_easy_cleanup(_curl);
        }
    }

    CURL* Get() {
        return _curl;
    }
private:
    CURL* _curl = nullptr;
};

base::DistribPoolThreadSafe<WriteDataMemory>& PrivateBufThreadSafe() {
    static base::NoDestructor<base::DistribPoolThreadSafe<WriteDataMemory>> instance;
    return *instance;
}

CURL* create_channel(base::DistribPoolThreadSafe<ScopedCURLHandle> &pool) {
    auto sh = pool.borrow();
    CURL* chan = nullptr;
    if (sh != nullptr) {
        chan = sh->Get();
        auto pri_ptr = PrivateBufThreadSafe().borrow();
        curl_easy_setopt(chan, CURLOPT_URL, "https://macx.net");
        //curl_easy_setopt(chan, CURLOPT_URL, "https://www.baidu.com");
        curl_easy_setopt(chan, CURLOPT_WRITEFUNCTION, on_write_callback);
        curl_easy_setopt(chan, CURLOPT_WRITEDATA, reinterpret_cast<void*>(pri_ptr));
        curl_easy_setopt(chan, CURLOPT_PRIVATE, pri_ptr);
        //curl_easy_setopt(chan, CURLOPT_WRITEDATA, pri_ptr);
        curl_easy_setopt(chan, CURLOPT_XFERINFODATA, pri_ptr);
        //curl_easy_setopt(chan, CURLOPT_FOLLOWLOCATION, 1L);
    }

    return chan;
}

void channel_thread_proc() {
    base::DistribPoolThreadSafe<ScopedCURLHandle> pool;
    if (!pool.init()) {
        baselog::error("curl handle pool failed");
        return;
    }

    do {
        auto chan = create_channel(pool);
        if (nullptr == chan) {
            baselog::error("create channel failed");
            break;
        }
        /* add the individual easy handle */
        auto ret = curl_multi_add_handle(g_curl_man, chan);
        std::this_thread::sleep_for(std::chrono::seconds(30));
        if (ret == CURLE_OK) {
            //ret = curl_multi_add_handle(g_curl_man, chan);
            curl_multi_wakeup(g_curl_man);
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    } while (1);
}

void service_thread_proc() {
    int still_running = 0;

    do {
        CURLMcode mc;
        int numfds;

        mc = curl_multi_perform(g_curl_man, &still_running);

        if (mc == CURLM_OK) {
            /* wait for activity or timeout */
            mc = curl_multi_poll(g_curl_man, NULL, 0, 10000, &numfds);
        }

        if (mc != CURLM_OK) {
            baselog::error("curl_multi failed, code {}", mc);
            break;
        }

        do {
            int msgq = 0;
            auto m = curl_multi_info_read(g_curl_man, &msgq);
            if (m == nullptr) {
                break;
            }

            auto chan = m->easy_handle;
            curl_multi_remove_handle(g_curl_man, m->easy_handle);

            WriteDataMemory* ptr_wdm = nullptr;
            curl_easy_getinfo(chan, CURLINFO_PRIVATE, &ptr_wdm);
            if (ptr_wdm != nullptr) {
                baselog::info("[ns] channel done: size= {}", ptr_wdm->buf.size());
                ptr_wdm->buf.clear();
            }
            PrivateBufThreadSafe().retrieve(ptr_wdm);
        } while (1);
    } while (1);
}

void main_multi_with_poll()
{
    if (!baselog::initialize(baselog::log_sink::windebug_sink)) {
        std::cout << "baselog init failed" << std::endl;
        return;
    }

    curl_global_init(CURL_GLOBAL_ALL);
    //std::thread thrd(thread_proc);
    if (!PrivateBufThreadSafe().init()) {
        baselog::error("private buf with not thread safe init failed.");
        return;
    }

    g_curl_man = curl_multi_init();

    std::thread service_thrd(service_thread_proc);
    std::thread channel_thrd(channel_thread_proc);
    service_thrd.join();
    channel_thrd.join();

    curl_multi_cleanup(g_curl_man);
    curl_global_cleanup();
    //auto chan = create_channel();
    //if (nullptr == chan) {
    //    baselog::error("create channel failed");
    //    return;
    //}
    /* add the individual easy handle */
    //auto ret = curl_multi_add_handle(curl_man, chan);

    //int still_running = 0;

    //do {
    //    CURLMcode mc;
    //    int numfds;

    //    mc = curl_multi_perform(curl_man, &still_running);

    //    if (mc == CURLM_OK) {
    //        /* wait for activity or timeout */
    //        mc = curl_multi_poll(curl_man, NULL, 0, 1000, &numfds);
    //    }

    //    if (mc != CURLM_OK) {
    //        fprintf(stderr, "curl_multi failed, code %d.\n", mc);
    //        break;
    //    }

    //} while (still_running);

    //auto p = PrivateBufNotThreadSafe().borrow();
    //curl_multi_remove_handle(curl_man, chan);
}
 
}

namespace util {
 
int run_multiple_form_data_request() 
{
    auto curl = curl_easy_init();
    if (curl == nullptr) {
        return 1;
    }
    auto mime = curl_mime_init(curl);
    if (mime == nullptr) {
        return 1;
    }
    //assigns multiple form data with mime stuff
    {
        auto part = curl_mime_addpart(mime);
        curl_mime_data(part, "xg's test title", CURL_ZERO_TERMINATED);
        curl_mime_name(part, "title");
        
        part = curl_mime_addpart(mime);
        curl_mime_data(part, "xg's test desc: It's the most important time", CURL_ZERO_TERMINATED);
        curl_mime_name(part, "desc");

        part = curl_mime_addpart(mime);
        curl_mime_data(part, "111", CURL_ZERO_TERMINATED);
        curl_mime_name(part, "config_id");

        /* send data from this file */
        part = curl_mime_addpart(mime);
        curl_mime_filedata(part, "test.zip");
        curl_mime_filename(part, "test.zip");
        curl_mime_name(part, "file");
    }
    //set headers
    curl_slist *list = nullptr;
    list = curl_slist_append(list, "Cookie: BAIDUID=424F28AF368D3B7AF5B3ADD383373CF2:FG=1");
    if (list == nullptr) {
        return 1;
    }

    curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
    curl_easy_setopt(curl, CURLOPT_URL, "http://mime-sh.baidu.com/sapi/v1/logsalvage/uploadlog?mock_uuid=123123123&_super_=1&env=2&cen=uc_oid_bduid&ua=bd_1080_2175_Redmi-K30-5G-picasso_9-3-5-16_a1&uc=gOLl8gIBv8_Ha2i3YuLe8gaqvtgjN28QxqqqB&cuid=1231231231|1&cfrom=ffffffff&from=ffffffff&rid=4312174411&mem=7787085824&is_simulator=0&oid=lavgNl8kLu0-t1Rm5tDCR0Ih3u5xiLMl0IHZu5i3XP46fSPw91HmA&bduid=_P258_u6-8GATqqqB&lus=34&umode=0&hotpatch_version=0&tm=1597030987160&cpt=0&sign=3F5D5BE051830DEA8F39362E954B9F35");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
    curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    return 0;
}

}

int _tmain(int argc, _TCHAR* argv[])
{
    return async_stuff::main_multi_with_poll();
}

