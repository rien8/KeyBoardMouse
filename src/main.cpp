#include <windows.h>

#include "app.h"

namespace {
const wchar_t* const kMutexName = L"Local\\mousekey.single.instance";
const wchar_t* const kShowMsgName = L"MouseKeyShowSettings";
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int)
{
    HANDLE mutex = CreateMutexW(nullptr, TRUE, kMutexName);
    const bool alreadyRunning =
        (mutex != nullptr && GetLastError() == ERROR_ALREADY_EXISTS);
    if (alreadyRunning) {
        const UINT showMsg = RegisterWindowMessageW(kShowMsgName);
        HWND hwnd = FindWindowW(kMouseKeyWndClass, nullptr);
        if (hwnd)
            PostMessageW(hwnd, showMsg, 0, 0);
        CloseHandle(mutex);
        return 0;
    }

    int rc = App::get().run(inst);

    if (mutex) {
        ReleaseMutex(mutex);
        CloseHandle(mutex);
    }
    return rc;
}
