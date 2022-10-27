// test_netcore.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "netcore_wrapper.h"

#include <iostream>
#include <chrono>
#include <atomic>

#include <base/log/logger.h>
#include <base/math/rand_util.h>

void on_call(netcore::NetResultType type, void* data, void* context)
{
    baselog::info("[main] user on_called successed: type= {}, data= {}, context= {}",
        (int)type, data, context);
    auto header = reinterpret_cast<netcore::NetResultHeader*>(data);
    auto progress = reinterpret_cast<netcore::NetResultProgress*>(data);
    auto finish = reinterpret_cast<netcore::NetResultFinish*>(data);
    auto response = reinterpret_cast<netcore::NetResultWrite*>(data);

    switch (type) {
    case netcore::NetResultType::NRT_ONCB_HEADER:
        baselog::info("[main] new header: {}", std::string(header->data, header->data_len));
        break;
    case netcore::NetResultType::NRT_ONCB_PROGRESS:
        baselog::info("[main] new progress: download= {}/{}@{:.2f} upload: {}/{}@{:.2f}",
            progress->download_transfered_size,
            progress->download_total_size,
            progress->download_speed,
            progress->upload_transfered_size,
            progress->upload_total_size,
            progress->upload_speed);
        break;
    case netcore::NetResultType::NRT_ONCB_WRITE:
        baselog::info("[main] new response data: len= {}", response->data_len);
        break;
    case netcore::NetResultType::NRT_ONCB_FINISH:
        baselog::info("[main] session finished: error= {}", (int)finish->result_code);
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
        auto ret = chan->post_request("https://example.com", std::bind(
            //auto ret = chan->send_request("https://macx.net", std::bind(
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
    
    if (!NetcoreWrapper::Instance().Initialize()) {
        baselog::error("load netcore failed");
        return 1;
    }
    baselog::debug("load netcore successed");

    std::vector<netcore::INetChannel*> chans;
    auto chan = NetcoreWrapper::Instance().NetServiceInstance()->create_channel();
    chan->enable_callback(netcore::NetResultType::NRT_ONCB_HEADER);
    chans.push_back(chan);
    //std::thread work_thread(thread_request, chan);
    //g_is_stop.store(true);
    //std::this_thread::sleep_for(std::chrono::milliseconds(5 * 1000));
    //chan->send_stop();

    //chan = NetcoreWrapper::Instance().NetServiceInstance()->create_channel();
    //chan->enable_callback(netcore::NetResultType::NRT_ONCB_PROGRESS);
    //chan->enable_callback(netcore::NetResultType::NRT_ONCB_WRITE);
    //chans.push_back(chan);
    baselog::info("send request start again...");
    auto ret = chan->send_request("https://google.com", std::bind(
       //auto ret = chan->post_request("https://freetestdata.com/wp-content/uploads/2021/09/Free_Test_Data_1OMB_MP3.mp3", std::bind(
        on_call, std::placeholders::_1, std::placeholders::_2,
        std::placeholders::_3), (void*)0x123456);
    baselog::info("send request end");
    //work_thread.join();
    baselog::info("remove channel...");

    char x = 0;
    std::cin >> x;
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

