// netcore.cpp : Defines the exported functions for the DLL application.
#include <include/netcore/netcore.h>

#include <base/log/logger.h>
#include <base/comm/immediate_crash.h>
#include <base/comm/no_destructor.h>
#include <base/comm/atomic_ref_count.h>

#include <src/net_service.h>

namespace netcore {
    
namespace {
static base::AtomicRefCount g_ref_count;
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
        {
            std::lock_guard<std::mutex> lg(_mutex);
            if (_is_inited) {
                return true;
            }

            if (!baselog::initialize(baselog::log_sink::windebug_sink)) {
                IMMEDIATE_CRASH();
            }

            auto ret = curl_global_init(CURL_GLOBAL_ALL);
            if (ret != CURLM_OK) {
                IMMEDIATE_CRASH();
            }

            _is_inited = true;
        }

        return true;
    }

    bool UnInitialize() {
        {
            std::lock_guard<std::mutex> lg(_mutex);
            if (!_is_inited) {
                return true;
            }

            curl_global_cleanup();

            if (!baselog::uninitialize()) {
                IMMEDIATE_CRASH();
            }

            _is_inited = false;
        }

        return true;
    }
private:
    std::mutex _mutex;
    bool _is_inited = false;
};
}

bool net_service_startup()
{
    //Must be very careful
    /*
    //https://curl.se/libcurl/c/curl_global_init.html
    curl_global_init - Global libcurl initialization
    If this is not thread-safe, you must not call this function when any other thread in the program (i.e. a thread sharing the same memory) is running. This does not just mean no other thread that is using libcurl. Because curl_global_init calls functions of other libraries that are similarly thread unsafe, it could conflict with any other thread that uses these other libraries.

    If you are initializing libcurl from a Windows DLL you should not initialize it from DllMain or a static initializer because Windows holds the loader lock during that time and it could cause a deadlock.
    */
    if (g_ref_count.Increment() == 0) {
        if (!CurlGlobalStuff::Instance().Initialize()) {
            IMMEDIATE_CRASH();
        }

        static base::NoDestructor<NetService> instance;
        g_static_net_service = instance.get();
    }


    if (g_static_net_service == nullptr) {
        IMMEDIATE_CRASH();
    }

    baselog::info("[ns] net service inited successed");
    if (!g_static_net_service->init()) {
        g_ref_count.Decrement();
        return false;
    }

    return true;
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

    if (!g_ref_count.Decrement()) {
        baselog::info("[ns] net service is shutdowning...");
        if (!g_static_net_service->close()) {
            IMMEDIATE_CRASH();
            return false;
        }

        baselog::info("[ns] net service shutdown done...");
        if (!CurlGlobalStuff::Instance().UnInitialize()) {
            IMMEDIATE_CRASH();
            return false;
        } 
    }

    return true;
}

}