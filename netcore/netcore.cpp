// netcore.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "netcore.h"
#include "net_core_imp.h"

#include <base/comm/no_destructor.h>

namespace netcore {
    
namespace {
    NetService* g_static_net_service = nullptr;
}

bool net_service_startup()
{
    static base::NoDestructor<NetService> instance;
    g_static_net_service = instance.get();

    if (g_static_net_service != nullptr) {
        return g_static_net_service->init();
    }

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

    return false;
}

}