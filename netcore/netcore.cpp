// netcore.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"

#include "netcore.h"

#include "net_service.h"
#include <base/log/logger.h>

#include <base/comm/immediate_crash.h>
#include <base/comm/no_destructor.h>

namespace netcore {
    
namespace {
    static NetService* g_static_net_service = nullptr;

    class CurlGlobalStuff {
    public:
        CurlGlobalStuff(){}
        ~CurlGlobalStuff() {}
        static CurlGlobalStuff& Instance() {
            static base::NoDestructor<CurlGlobalStuff> instance;
            return *instance;
        }
        bool Initialize() {
            if (!baselog::initialize(baselog::log_sink::windebug_sink)) {
                IMMEDIATE_CRASH();
            }
            auto ret = curl_global_init(CURL_GLOBAL_ALL);
            baselog::info("[ns] libcurl global init result= {}", ret);
            if (ret != CURLM_OK) {
                baselog::fatal("[ns] libcurl global init failed");
            }
            return (ret == CURLM_OK);
        }
        bool UnInitialize() {
            curl_global_cleanup();
            baselog::info("[ns] libcurl global uninitialize");
            return baselog::uninitialize();
        }
    };
}

bool net_service_startup()
{
    if (!CurlGlobalStuff::Instance().Initialize()) {
        return false;
    }

    static base::NoDestructor<NetService> instance;
    g_static_net_service = instance.get();

    if (g_static_net_service != nullptr) {
        return g_static_net_service->init();
    }

    baselog::fatal("[ns] net service inited failed");
    return false;
}

INetService* net_service_instance()
{
    return g_static_net_service;
} 

bool net_service_shutdown(INetService* netservice)
{
    if (g_static_net_service == nullptr) {
        return false;
    }

    if (netservice != g_static_net_service && netservice != nullptr) {
        return false;
    }

    baselog::info("[ns] net service is shutdowning...");
    if (!g_static_net_service->close()) {
        return false;
    }

    baselog::info("[ns] net service shutdown done...");
    if (!CurlGlobalStuff::Instance().UnInitialize()) {
        return false;
    }

    return true;
}

}