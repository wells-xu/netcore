// test_netcore.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "netcore_wrapper.h"

#include <iostream>

#include <base/log/logger.h>

void on_call(int type, void *data, void *data2, void *data3)
{
    baselog::info("[main] on_called...");
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
    if (chan == nullptr) {
        baselog::error("create channel failed");
        return 1;
    }

    auto ret = chan->post_request( "www.baidu.com", std::bind(
        on_call, std::placeholders::_1, std::placeholders::_2,
        std::placeholders::_3, std::placeholders::_4));
    baselog::info("new request had posted");
    char x = 0;
    std::cin >> x;
    baselog::info("netcore tester exited safe");
    if (!NetcoreWrapper::Instance().UnInitialize()) {
        baselog::error("net service uninit failed");
        return 1;
    }

	return 0;
}

