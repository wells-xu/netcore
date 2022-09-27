#pragma once
#include <functional>
#include <string>
#include "file.h"

namespace base {
namespace sys {

class DllClient {
public:
    DllClient() :
        _dll_handle(NULL),
        _last_error(0)  {
    }
    ~DllClient() {
        if (is_valid()) {
            ::FreeLibrary(_dll_handle);
        }
    }

    bool load(const std::wstring &dll_path) {
        if (is_valid()) {
            return false;
        }

        if (dll_path.empty()) {
            return false;
        }

        _dll_handle = base::sys::file::LoadLibrarySecurely(dll_path.c_str());
        _last_error = ::GetLastError();
        return is_valid();
    }

    bool unload() {
        if (!is_valid()) {
            return false;
        }

        if (0 == ::FreeLibrary(_dll_handle)) {
            _last_error = ::GetLastError();
            return false;
        }

        return true;
    }

    bool is_valid() { return (_dll_handle != NULL); }

    template<typename Func>
    std::function<Func> get_dll_func(const std::string& func_name) {
        if (_dll_handle != NULL) {
            auto dll_func = ::GetProcAddress(_dll_handle, func_name.c_str());
            if (dll_func) {
                return std::function<Func>(
                    reinterpret_cast<Func*>(dll_func));
            }
        }
        return std::function<Func>();
    }
private:
    HMODULE _dll_handle;
    DWORD _last_error;
    std::wstring _current_path;
};

}
}

