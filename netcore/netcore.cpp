// netcore.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "netcore.h"
#include "net_core_imp.h"

namespace netcore {

INetService* net_service_startup()
{
    static NetService g_static_net_service;
    if (!g_static_net_service.init()) {
        return nullptr;
    }

    return &g_static_net_service;
}

bool net_service_shutdown(INetService* netservice)
{
    if (netservice != nullptr) {
        return netservice->close();
    }

    return false;
}

}