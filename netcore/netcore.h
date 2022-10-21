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

static const int TIMEOUT_MS_INFINITE = 0;
typedef std::function<void(int type, void* data, void* data2, void* data3)> CallbackType;

class INetService {
public:
    virtual bool init() = 0;
    virtual bool close() = 0;

    virtual INetChannel* create_channel() = 0;
    virtual INetChannel* create_clone_channel(INetChannel*) = 0;
    virtual void         remove_channel(INetChannel*) = 0;
};

class INetChannel {
public:
    virtual bool set_header(const std::string& header) = 0;
    virtual bool set_body(const std::string& body) = 0;
    virtual bool set_cookie(const std::string& cookie) = 0;
    virtual bool set_mime_data(const std::string &part_name_utf8,
        const std::string &file_data_utf8, const std::string &part_type = "") = 0;
    virtual bool set_mime_file(const std::string &part_name_utf8,
        const std::string &file_path_utf8, const std::string &remote_name_utf8,
        const std::string &part_type = "") = 0;

    virtual bool post_request(const std::string &url,
        CallbackType callback = 0, void *context = 0,
        int timeout_ms = TIMEOUT_MS_INFINITE) = 0;
    virtual bool send_request(const std::string &url,
        CallbackType callback = 0, void *context = 0,
        int timeout_ms = TIMEOUT_MS_INFINITE) = 0;

    virtual void send_stop() = 0;
    virtual void post_stop() = 0;

    //virtual void on_callback() = 0;
};

}
