#pragma once

#include <functional>
#include <memory>
#include <string>
#include <atomic>
#include <mutex>

#include <netcore/netcore.h>
#include <base/system/dll_client_def.h>

namespace base {
namespace sys {
class DllClient;
}
}

class NetcoreWrapper
{
public: 
    NetcoreWrapper();
    ~NetcoreWrapper();

    static NetcoreWrapper& Instance();

    bool Initialize();
    netcore::INetService* NetServiceInstance();
    bool UnInitialize();

private:
    FINAL_FUNC_STUFF_DECLARE(net_service_startup, bool);
    FINAL_FUNC_STUFF_DECLARE(net_service_instance, netcore::INetService*);
    FINAL_FUNC_STUFF_DECLARE(net_service_shutdown, bool, netcore::INetService*);
    
    netcore::INetService* _net_service = nullptr;
    std::mutex _mutex; 
    std::unique_ptr<base::sys::DllClient> _dll_client;
};

