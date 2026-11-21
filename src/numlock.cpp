#include "numlock.h"

#include <windows.h>

namespace numlock {

bool isOn()
{
    return (GetKeyState(VK_NUMLOCK) & 0x0001) != 0;
}

bool turnOff()
{
    if (!isOn())
        return false;

    INPUT in[2] = {};
    for (int i = 0; i < 2; ++i) {
        in[i].type = INPUT_KEYBOARD;
        in[i].ki.wVk = VK_NUMLOCK;
        in[i].ki.wScan = 0x45;
        in[i].ki.dwFlags = (i == 1) ? KEYEVENTF_KEYUP : 0;
    }
    SendInput(2, in, sizeof(INPUT));
    return true;
}

} // namespace numlock
