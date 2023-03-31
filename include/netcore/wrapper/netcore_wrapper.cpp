#include "netcore_wrapper.h"

#include <base/system/dll_client_win.h>
#include <base/files/path.h>

#include <base/comm/no_destructor.h>

NetcoreWrapper::NetcoreWrapper()
{
}

NetcoreWrapper::~NetcoreWrapper()
{
}

NetcoreWrapper& NetcoreWrapper::Instance()
{
    static base::NoDestructor<NetcoreWrapper> instance;
    return *instance;
}

bool NetcoreWrapper::Initialize()
{
    {
        std::lock_guard<std::mutex> lg(_mutex);
        if (_net_service != nullptr) {
            return true;
        }
    }

    std::wstring dll_path;
    if (!base::module::get_module_dir(dll_path, nullptr)) {
        return false;
    }

#ifdef _WIN64
    if (!base::path::path_append(L"netcore_x64.dll", dll_path)) {
        return false;
    }
#else
    if (!base::path::path_append(L"netcore.dll", dll_path)) {
        return false;
    }
#endif

    if (!base::path::path_is_file_exist(dll_path)) {
        return false;
    }

    auto client_ptr = std::make_unique<base::sys::DllClient>();
    if (!client_ptr->load(dll_path.c_str())) {
        return false;
    }

    GET_DLL_FUNC(client_ptr, net_service_startup);
    GET_DLL_FUNC(client_ptr, net_service_instance);
    GET_DLL_FUNC(client_ptr, net_service_shutdown); 

    if (!MEMBER_FUNCTION_NAME(net_service_startup)) {
        return false;
    }

    if (!MEMBER_FUNCTION_NAME(net_service_instance)) {
        return false;
    }

    if (!MEMBER_FUNCTION_NAME(net_service_startup)()) {
        return false;
    }

    auto service_ptr = MEMBER_FUNCTION_NAME(net_service_instance)();
    if (service_ptr == nullptr) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lg(_mutex);
        _net_service = service_ptr;
        this->_dll_client = std::move(client_ptr);
    }

    return true;
}

netcore::INetService* NetcoreWrapper::NetServiceInstance()
{
    std::lock_guard<std::mutex> lg(_mutex);
    return _net_service;
}

bool NetcoreWrapper::UnInitialize()
{
    {
        std::lock_guard<std::mutex> lg(_mutex);
        if (_net_service == nullptr) {
            return false;
        }
    }

    if (!MEMBER_FUNCTION_NAME(net_service_shutdown)) {
        return false;
    }
    
    if (!MEMBER_FUNCTION_NAME(net_service_shutdown)(_net_service)) {
        return false;
    }
    
    {
        std::lock_guard<std::mutex> lg(_mutex);
        _net_service = nullptr;
        _dll_client.reset();
    }

    return true;
}

