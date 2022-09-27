#include "stdafx.h"
#include "netcore_wrapper.h"

#include "dll_client.h"

NetcoreWrapper::NetcoreWrapper() :
    _net_service(nullptr),
    _dll_client(new base::sys::DllClient())
{
}


NetcoreWrapper::~NetcoreWrapper()
{
    stop(_net_service);
}

bool NetcoreWrapper::start(netcore::INetService** ins)
{
    if (ins == nullptr) {
        return false;
    }

    if (_net_service != nullptr) {
        return false;
    }

    std::wstring dll_path;
    base::sys::path::path_current_with_spec(dll_path, L"netcore.dll");

    if (!_dll_client->load(dll_path.c_str())) {
		return false;
    }

    _func_start = _dll_client->get_dll_func<dllfunc::LPFUNC_NETCORE_STARTUP>("net_service_startup");
    if (!_func_start) {
        return false;
    }
 
    _func_stop = _dll_client->get_dll_func<dllfunc::LPFUNC_NETCORE_SHUTDOWN>("net_service_shutdown");
    if (!_func_stop) {
        return false;
    }
 
    _net_service = _func_start();
    *ins = _net_service;
    return (*ins != nullptr);
}

bool NetcoreWrapper::stop(netcore::INetService* ins)
{
    if (_net_service == nullptr) {
        return false;
    }
    if (ins == _net_service) {
        _net_service = nullptr;
        if (!_func_stop) {
            return false;
        }
        return _func_stop(ins);
    }

    return false;
}
