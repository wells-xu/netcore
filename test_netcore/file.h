#pragma once
#include <string>
#include <Windows.h>

namespace base {
namespace sys {

namespace path {
    bool current_module_dir(std::wstring &out_path);
    bool path_append(std::wstring &pre_path, const std::wstring& spec_path);
    bool path_current_with_spec(std::wstring &out_path, const std::wstring& spec_path);
}

namespace file {
    HMODULE LoadLibrarySecurely(LPCTSTR path);
}

}
}
