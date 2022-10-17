// netcore.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "netcore.h"
#include "net_service.h"
#include <base/log/logger.h>

#include <base/comm/immediate_crash.h>
#include <base/comm/no_destructor.h>
#include <base/comm/lazy_instance.h>

namespace netcore {
    
namespace {
    NetService* g_static_net_service = nullptr;

    class CurlGlobalStuff {
    public:
        CurlGlobalStuff() {
            if (!baselog::initialize(baselog::log_sink::windebug_sink)) {
                IMMEDIATE_CRASH();
            }
            auto ret = curl_global_init(CURL_GLOBAL_ALL);
            baselog::info("[ns] libcurl global init result= {}", ret);
            if (ret != CURLM_OK) {
                baselog::fatal("[ns] libcurl global init failed");
            }

        }
        ~CurlGlobalStuff() {
            curl_global_cleanup();
            baselog::info("[ns] libcurl global uninitialize");
            baselog::uninitialize();
        }

        static CurlGlobalStuff& MarkInstance() {
            static CurlGlobalStuff instance;
            return instance;
        }
    };
}

bool net_service_startup()
{
    CurlGlobalStuff::MarkInstance();

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
    if (netservice != g_static_net_service && netservice != nullptr) {
        return false;
    }

    if (g_static_net_service) {
        return g_static_net_service->close();
    }

    baselog::error("[ns] net service shutdown failed");
    return false;
}

}