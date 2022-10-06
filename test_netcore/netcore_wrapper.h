#pragma once
#include <functional>
#include <memory>
#include <string>
#include <netcore/netcore.h>

//namespace netcore {
//    class INetService;
//}

namespace dllfunc {
    typedef bool (__cdecl LPFUNC_NETCORE_STARTUP) \
        ();
    typedef netcore::INetService* (__cdecl LPFUNC_NETCORE_SERVICE_INSTANCE) \
        ();
    typedef bool(__cdecl LPFUNC_NETCORE_SHUTDOWN) \
        (netcore::INetService*);
}

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
    std::function<dllfunc::LPFUNC_NETCORE_STARTUP> _func_start;
    std::function<dllfunc::LPFUNC_NETCORE_SERVICE_INSTANCE> _func_instance;
    std::function<dllfunc::LPFUNC_NETCORE_SHUTDOWN> _func_stop; 
    
    netcore::INetService* _net_service = nullptr;
    std::unique_ptr<base::sys::DllClient> _dll_client;
};

