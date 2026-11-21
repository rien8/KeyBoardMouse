#include <windows.h>
#include <cstdio>
#include <cstdlib>

namespace {

const char* eventName(WPARAM wp)
{
    switch (wp) {
    case WM_LBUTTONDOWN: return "LEFT_DOWN";
    case WM_LBUTTONUP: return "LEFT_UP";
    case WM_RBUTTONDOWN: return "RIGHT_DOWN";
    case WM_RBUTTONUP: return "RIGHT_UP";
    case WM_MBUTTONDOWN: return "MIDDLE_DOWN";
    case WM_MBUTTONUP: return "MIDDLE_UP";
    case WM_MOUSEWHEEL: return "WHEEL";
    default: return nullptr;
    }
}

LRESULT CALLBACK mouseProc(int code, WPARAM wp, LPARAM lp)
{
    if (code == HC_ACTION) {
        if (const char* name = eventName(wp)) {
            printf("%s\n", name);
            fflush(stdout);
        }
    }
    return CallNextHookEx(nullptr, code, wp, lp);
}

} // namespace

int wmain(int argc, wchar_t** argv)
{
    const int ms = (argc > 1) ? (int)wcstol(argv[1], nullptr, 10) : 5000;
    HHOOK hook = SetWindowsHookExW(WH_MOUSE_LL, mouseProc, nullptr, 0);
    if (!hook) {
        wprintf(L"SetWindowsHookEx failed: %lu\n", GetLastError());
        return 1;
    }
    printf("READY\n");
    fflush(stdout);

    const DWORD deadline = GetTickCount() + (DWORD)ms;
    MSG msg;
    while ((int)(deadline - GetTickCount()) > 0) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
            TranslateMessage(&msg), DispatchMessageW(&msg);
        Sleep(5);
    }
    UnhookWindowsHookEx(hook);
    printf("DONE\n");
    return 0;
}
