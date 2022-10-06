#include "stdafx.h"
#include "netcore_wrapper.h"

#include "dll_client.h"

#include <base/comm/no_destructor.h>

NetcoreWrapper::NetcoreWrapper() :
    _net_service(nullptr),
    _dll_client(new base::sys::DllClient())
{
}


NetcoreWrapper::~NetcoreWrapper()
{
    //UnInitialize();
}

NetcoreWrapper& NetcoreWrapper::Instance()
{
    static base::NoDestructor<NetcoreWrapper> instance;
    return *instance;
}

bool NetcoreWrapper::Initialize()
{
    if (_net_service != nullptr) {
        return false;
    }

    std::wstring dll_path;
    base::sys::path::path_current_with_spec(dll_path, L"netcore.dll");

    if (!_dll_client->load(dll_path.c_str())) {
		return false;
    }

    _func_start = _dll_client->get_dll_func<
        dllfunc::LPFUNC_NETCORE_STARTUP>("net_service_startup");
    if (!_func_start) {
        return false;
    }

    _func_instance = _dll_client->get_dll_func<
        dllfunc::LPFUNC_NETCORE_SERVICE_INSTANCE>("net_service_instance");
    if (!_func_instance) {
        return false;
    }
 
    _func_stop = _dll_client->get_dll_func<
        dllfunc::LPFUNC_NETCORE_SHUTDOWN>("net_service_shutdown");
    if (!_func_stop) {
        return false;
    }

    if (!_func_start()) {
        return false;
    }

    _net_service = _func_instance();
    if (_net_service == nullptr) {
        return false;
    }

    return true;
}

netcore::INetService* NetcoreWrapper::NetServiceInstance()
{
    return _net_service;
}

bool NetcoreWrapper::UnInitialize()
{
    if (_net_service == nullptr) {
        return false;
    }

    if (!_func_stop) {
        return false;
    }

    return _func_stop(_net_service);
}
