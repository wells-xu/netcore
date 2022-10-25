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

        baselog::info("send request start...");
        auto ret = chan->send_request("https://example.com", std::bind(
            //auto ret = chan->send_request("https://macx.net", std::bind(
            on_call, std::placeholders::_1, std::placeholders::_2,
            std::placeholders::_3));
        baselog::info("send request end");
        std::this_thread::sleep_for(std::chrono::milliseconds(1000 * 10));

        baselog::info("send request start again...");
        ret = chan->send_request("https://example.com", std::bind(
            //auto ret = chan->send_request("https://macx.net", std::bind(
            on_call, std::placeholders::_1, std::placeholders::_2,
            std::placeholders::_3));
        baselog::info("send request end");

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
    //std::thread work_thread(thread_request, chan);
    //g_is_stop.store(true);
    //std::this_thread::sleep_for(std::chrono::milliseconds(5 * 1000));
    //chan->send_stop();

    baselog::info("send request start...");
    //auto ret = chan->send_request("https://example.com", std::bind(
        auto ret = chan->send_request("https://macx.net", std::bind(
        on_call, std::placeholders::_1, std::placeholders::_2,
        std::placeholders::_3));
    baselog::info("send request end");
    //NetcoreWrapper::Instance().NetServiceInstance()->remove_channel(chan);

    std::this_thread::sleep_for(std::chrono::milliseconds(1000 * 5));
    //chan = NetcoreWrapper::Instance().NetServiceInstance()->create_channel();
    baselog::info("send request start again...");
    ret = chan->send_request("https://example.com", std::bind(
        //ret = chan->send_request("https://macx.net", std::bind(
        on_call, std::placeholders::_1, std::placeholders::_2,
        std::placeholders::_3));
    baselog::info("send request end");
    char x = 0;
    std::cin >> x;
    //work_thread.join();
    baselog::info("remove channel...");
    NetcoreWrapper::Instance().NetServiceInstance()->remove_channel(chan);
    if (!NetcoreWrapper::Instance().UnInitialize()) {
        baselog::error("net service uninit failed");
        return 1;
    }

    baselog::info("all done");
	return 0;
}

