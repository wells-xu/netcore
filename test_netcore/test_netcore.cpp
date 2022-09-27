// test_netcore.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <iostream>
#include "netcore_wrapper.h"

void on_call(int type, void *data, void *data2, void *data3)
{
    std::cout << "[main] on_called..." << std::endl;
}

int _tmain(int argc, _TCHAR* argv[])
{
    NetcoreWrapper nw;
    netcore::INetService* ns_ptr = nullptr;
    if (!nw.start(&ns_ptr)) {
        std::cerr << "load netcore failed" << std::endl;
        return 1;
    }
    std::cerr << "load netcore successed: " << std::hex << ns_ptr << std::endl;

    auto chan = ns_ptr->create_channel();
    if (chan == nullptr) {
        std::cerr << "create channel failed" << std::endl;
        return 1;
    }

    auto ret = chan->post_request( "www.baidu.com", std::bind(
        on_call, std::placeholders::_1, std::placeholders::_2,
        std::placeholders::_3, std::placeholders::_4));
    char x = 0;
    std::cin >> x;
	return 0;
}

