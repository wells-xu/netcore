// netcore.cpp : Defines the exported functions for the DLL application.
//
#pragma once

#include "stdafx.h"
#include <string>
#include <functional>
 
#ifdef NETCORE_EXPORTS
#define DLL_NETCORE_API extern "C" __declspec(dllexport)
#else
#define DLL_NETCORE_API __declspec(dllimport)
#endif

namespace netcore {

class INetService;
class INetChannel;

DLL_NETCORE_API bool         net_service_startup();
DLL_NETCORE_API INetService* net_service_instance();
DLL_NETCORE_API bool         net_service_shutdown(INetService*);

class INetService {
public:
    virtual bool init() = 0;
    virtual bool close() = 0;

    virtual INetChannel* create_channel() = 0;
    virtual void         remove_channel(INetChannel*) = 0;
};

class INetChannel {
public:
    virtual bool init() = 0;

    virtual bool set_header(const std::string& header) = 0;
    virtual bool set_body() = 0;
    virtual bool set_cookie() = 0;

    typedef std::function<void(int type, void *data, void *data2, void *data3)> CallbackType;
    virtual bool post_request(const std::string &url, CallbackType callback = 0, void *param = 0, int timeout_ms = -1) = 0;
    virtual bool send_request(const std::string& url, CallbackType callback, void* param) = 0;

    virtual void send_stop() = 0;
    virtual void post_stop() = 0;
    virtual bool add_form_data(const std::string &name, const std::string &content) = 0;
    virtual bool add_form_file(const std::string &name, const std::string &content, const std::string &filename) = 0;

    virtual void on_callback() = 0;
};

}
