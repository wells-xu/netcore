// test_curl.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <thread>
#include <iostream>
#include <fstream>
#include <curl\curl.h>

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
    std::cout << "on_write_callback: " << tid << std::endl;

    ptr->buf.append(reinterpret_cast<char*>(buffer), size * nmemb);
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
        //curl_easy_setopt(curl, CURLOPT_URL, "https://www.baidu.com");
        curl_easy_setopt(curl, CURLOPT_URL, "https://www.hao123.com");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, on_write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, reinterpret_cast<void*>(&wdm));
        //curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
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

void thread_proc()
{
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
    return async_stuff::run();
}

