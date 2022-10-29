// test_netcore.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "netcore_wrapper.h"

#include <iostream>
#include <chrono>
#include <atomic>

#include <base/log/logger.h>
#include <base/math/rand_util.h>

#define SAFE_STRING_PRINTF(p) \
    (p == nullptr ? ("") : (p))

void on_call(netcore::NetResultType type, void* data, void* context)
{
    baselog::info("[user_call] user on_called successed: type= {}, data= {}, context= {}",
        (int)type, data, context);
    auto header = reinterpret_cast<netcore::NetResultHeader*>(data);
    auto progress = reinterpret_cast<netcore::NetResultProgress*>(data);
    auto finish = reinterpret_cast<netcore::NetResultFinish*>(data);
    auto response = reinterpret_cast<netcore::NetResultWrite*>(data);

    switch (type) {
    case netcore::NetResultType::NRT_ONCB_HEADER:
        baselog::info("[user_call] new header: {}", std::string(header->data, (std::size_t)header->data_len));
        break;
    case netcore::NetResultType::NRT_ONCB_PROGRESS:
        baselog::info("[user_call] new progress: download= {}/{}@{:.2f} upload: {}/{}@{:.2f}",
            progress->download_transfered_size,
            progress->download_total_size,
            progress->download_speed,
            progress->upload_transfered_size,
            progress->upload_total_size,
            progress->upload_speed);
        break;
    case netcore::NetResultType::NRT_ONCB_WRITE:
        baselog::info("[user_call] new response data: len= {}", response->data_len);
        break;
    case netcore::NetResultType::NRT_ONCB_FINISH:
        baselog::info("[user_call] http finished message: ");
        baselog::info("[user_call] result_code= {}", (int)finish->result_code);
        baselog::info("[user_call] response_code= {}", finish->http_response_code);
        baselog::info("[user_call] content_type= {}", SAFE_STRING_PRINTF(finish->http_content_type));
        baselog::info("[user_call] content_length= {}", finish->content_length_download);
        baselog::info("[user_call] header_length= {}", finish->http_header_len);
        baselog::info("[user_call] data_length= {}", finish->data_len);
        baselog::info("[user_call] app_average_spped= {}", finish->app_average_speed);
        baselog::info("[user_call] download_spped= {}", finish->download_speed_bytes_persecond);
        baselog::info("[user_call] client ip string= {}", SAFE_STRING_PRINTF(finish->primary_ip_string));
        baselog::info("[user_call] namelookup time = {} ms", finish->namelookup_time_ms);
        baselog::info("[user_call] connected time = {} ms", finish->connected_time_ms);
        baselog::info("[user_call] appconnect time = {} ms", finish->app_connected_time_ms);
        baselog::info("[user_call] pretransfer time = {} ms", finish->pretransfer_time_ms);
        baselog::info("[user_call] starttransfer time = {} ms", finish->starttransfer_time_ms);
        baselog::info("[user_call] total time= {} ms", finish->total_time_ms);
        baselog::info("[user_call] redirect_count= {}", finish->redirect_count);
        baselog::info("[user_call] redirect_time= {} ms", finish->redirect_time_ms);
        break;
    default:
        break;
    }
}

std::atomic_bool g_is_stop {false};
void thread_request(netcore::INetChannel *chan)
{
    while (!g_is_stop) {
        if (chan == nullptr) {
            baselog::error("create channel failed");
            return;
        }

        //baselog::trace("[ns] ready to  post a request...");
        //auto ret = chan->send_request("https://example.com", std::bind(
        ////auto ret = chan->send_request("https://macx.net", std::bind(
        //    on_call, std::placeholders::_1, std::placeholders::_2,
        //    std::placeholders::_3));
        //baselog::info("new request has done: {}", ret);
        //auto ru64 = base::RandUint64() % 3000;
        //baselog::trace("requesting done with some sleep: {}ms", ru64);
        //std::this_thread::sleep_for(std::chrono::milliseconds(ru64));
        //std::this_thread::sleep_for(std::chrono::milliseconds(1000 * 10));

        baselog::info("post request start...");
        //auto ret = chan->post_request("https://example.com", std::bind(
            auto ret = chan->send_request("https://macx.net", std::bind(
            on_call, std::placeholders::_1, std::placeholders::_2,
            std::placeholders::_3), (void*)chan);
        baselog::info("post request end");
        break;
    }
}

int _tmain(int argc, _TCHAR* argv[])
{
    if (!baselog::initialize()) {
        baselog::error("baselog init failed");
        return 1;
    }
    auto is_r = baselog::set_logger_level(baselog::log_level::info);

    if (!NetcoreWrapper::Instance().Initialize()) {
        baselog::error("load netcore failed");
        return 1;
    }
    baselog::debug("load netcore successed");

    std::vector<netcore::INetChannel*> chans;
    auto chan = NetcoreWrapper::Instance().NetServiceInstance()->create_channel();
    chan->enable_callback(netcore::NetResultType::NRT_ONCB_PROGRESS);
    chan->enable_callback(netcore::NetResultType::NRT_ONCB_HEADER);
    chans.push_back(chan);
    std::thread work_thread(thread_request, chan);
    //g_is_stop.store(true);

    chan = NetcoreWrapper::Instance().NetServiceInstance()->create_channel();
    chan->enable_callback(netcore::NetResultType::NRT_ONCB_PROGRESS);
    chan->enable_callback(netcore::NetResultType::NRT_ONCB_WRITE);
    //chans.push_back(chan);
    baselog::info("send request start again...");
    auto ret = chan->send_request("https://youtube.com", std::bind(
       //auto ret = chan->send_request("https://freetestdata.com/wp-content/uploads/2021/09/Free_Test_Data_1OMB_MP3.mp3", std::bind(
       //auto ret = chan->post_request("https://macx.net", std::bind(
        on_call, std::placeholders::_1, std::placeholders::_2,
        std::placeholders::_3), (void*)0x123456);
    baselog::info("send request end");
    work_thread.join();
    baselog::info("remove channel...");

    //std::this_thread::sleep_for(std::chrono::milliseconds(5 * 1000));
    //chan->send_stop();

    //char x = 0;
    //std::cout << "net requests all done" << std::endl;
    //std::cin >> x;
    for (auto i : chans) {
        NetcoreWrapper::Instance().NetServiceInstance()->remove_channel(i);
    }
    if (!NetcoreWrapper::Instance().UnInitialize()) {
        baselog::error("net service uninit failed");
        return 1;
    }

    baselog::info("all done");
	return 0;
}

