#include "autostart.h"

#include <windows.h>
#include <cstdio>

namespace {

const wchar_t* const kRunKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const wchar_t* const kValueName = L"mousekey";

} // namespace

namespace autostart {

bool isEnabled()
{
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
        return false;

    wchar_t buf[1024];
    DWORD size = sizeof(buf);
    DWORD type = 0;
    const LONG rc = RegQueryValueExW(key, kValueName, nullptr, &type,
                                     reinterpret_cast<LPBYTE>(buf), &size);
    RegCloseKey(key);

    if (rc != ERROR_SUCCESS || size < sizeof(wchar_t))
        return false;
    return type == REG_SZ || type == REG_EXPAND_SZ;
}

bool setEnabled(bool on)
{
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS)
        return false;

    LONG rc = ERROR_SUCCESS;
    if (on) {
        wchar_t exe[MAX_PATH] = {};
        const DWORD n = GetModuleFileNameW(nullptr, exe, MAX_PATH);
        if (n == 0 || n >= MAX_PATH) {
            RegCloseKey(key);
            return false;
        }

        wchar_t value[MAX_PATH + 4] = {};
        swprintf_s(value, L"\"%s\"", exe);
        rc = RegSetValueExW(key, kValueName, 0, REG_SZ,
                            reinterpret_cast<const BYTE*>(value),
                            static_cast<DWORD>((wcslen(value) + 1) * sizeof(wchar_t)));
    } else {
        rc = RegDeleteValueW(key, kValueName);
        if (rc == ERROR_FILE_NOT_FOUND)
            rc = ERROR_SUCCESS;
    }

    RegCloseKey(key);
    return rc == ERROR_SUCCESS;
}

} // namespace autostart
