#include <windows.h>
#include <cstdio>
#include <cstdlib>

int wmain(int argc, wchar_t** argv)
{
    if (argc < 3) {
        wprintf(L"usage: injecthold <scan_code> <hold_ms>\n");
        return 2;
    }
    const unsigned short scan = (unsigned short)wcstoul(argv[1], nullptr, 0);
    const int holdMs = (int)wcstol(argv[2], nullptr, 10);
    if (scan == 0 || scan > 0x7F || holdMs <= 0) {
        wprintf(L"invalid args\n");
        return 2;
    }

    INPUT down = {};
    down.type = INPUT_KEYBOARD;
    down.ki.wScan = scan;
    down.ki.dwFlags = KEYEVENTF_SCANCODE;
    INPUT up = down;
    up.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;

    if (SendInput(1, &down, sizeof(INPUT)) != 1) {
        wprintf(L"SendInput down failed: %lu\n", GetLastError());
        return 1;
    }
    Sleep(holdMs);
    if (SendInput(1, &up, sizeof(INPUT)) != 1) {
        wprintf(L"SendInput up failed: %lu\n", GetLastError());
        return 1;
    }
    wprintf(L"injected scan 0x%X for %d ms\n", scan, holdMs);
    return 0;
}
