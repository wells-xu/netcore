#include "stdafx.h"
#include "file.h"
#include <Shlwapi.h>

namespace base {
namespace sys {

bool path::current_module_dir(std::wstring &out_path)
{
    TCHAR tmp_path[MAX_PATH + 1] = { 0 };
    if (0 == ::GetModuleFileName(NULL, tmp_path, MAX_PATH)) {
        return false;
    }

    ::PathRemoveFileSpec(tmp_path);
    out_path = tmp_path;
    return true;
}

bool path::path_append(std::wstring &pre_path, const std::wstring &spec_path)
{
    if (pre_path.empty() || spec_path.empty()) {
        return false;
    }

    TCHAR tmp_path[MAX_PATH + 1] = { 0 };
    if (0 != wcscpy_s(tmp_path, _countof(tmp_path), pre_path.c_str())) {
        return false;
    }
    if (!::PathAppend(tmp_path, spec_path.c_str())) {
        return false;
    }

    pre_path.assign(tmp_path);
    return true;
}

bool path::path_current_with_spec(std::wstring &out_path, const std::wstring& spec_path)
{
    if (!current_module_dir(out_path)) {
        return false;
    }

    return path_append(out_path, spec_path);
}

HMODULE file::LoadLibrarySecurely(LPCTSTR path)
{
    if (path == nullptr) {
        return NULL;
    }

    if (::PathIsRelative(path)) {
        return NULL;
    }

    if (!::PathFileExists(path)) {
        return NULL;
    }

    //if (!HashBaiduDigitalSign(path)) {
    //    return NULL;
    //}

    return ::LoadLibrary(path);
}

}
}
