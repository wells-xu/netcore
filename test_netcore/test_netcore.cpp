// test_netcore.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "netcore_wrapper.h"

#include <iostream>
#include <chrono>
#include <atomic>

#include <base/log/logger.h>

void on_call(int type, void *data, void *data2, void *data3)
{
    baselog::info("[main] on_called...");
}

std::atomic_bool g_is_stop {false};
void thread_request(netcore::INetChannel *chan)
{
    while (!g_is_stop) {
        if (chan == nullptr) {
            baselog::error("create channel failed");
            return;
        }

        baselog::trace("[ns] ready to  post a request...");
        auto ret = chan->send_request("https://example.com", std::bind(
            on_call, std::placeholders::_1, std::placeholders::_2,
            std::placeholders::_3, std::placeholders::_4));
        baselog::info("new request had posted: {}", ret);
        //std::this_thread::sleep_for(std::chrono::milliseconds(10 * 1000));

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

    auto chan = NetcoreWrapper::Instance().NetServiceInstance()->create_channel();
    std::thread work_thread(thread_request, chan);
    //char x = 0;
    //std::cin >> x;
    //g_is_stop.store(true);
    baselog::info("waiting send stop...");
    std::this_thread::sleep_for(std::chrono::milliseconds(5 * 1000));
    //chan->send_stop();
    work_thread.join();
    baselog::info("remove channel...");
    NetcoreWrapper::Instance().NetServiceInstance()->remove_channel(chan);
    if (!NetcoreWrapper::Instance().UnInitialize()) {
        baselog::error("net service uninit failed");
        return 1;
    }

    baselog::info("all done");
	return 0;
}

