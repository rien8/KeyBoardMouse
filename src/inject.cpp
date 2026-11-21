#include "inject.h"

#include <windows.h>

namespace {

INPUT makeMouse(DWORD flags, int dx, int dy, int data)
{
    INPUT in = {};
    in.type = INPUT_MOUSE;
    in.mi.dx = dx;
    in.mi.dy = dy;
    in.mi.mouseData = (DWORD)(ULONG_PTR)(DWORD)data;
    in.mi.dwFlags = flags;
    in.mi.time = 0;
    in.mi.dwExtraInfo = 0;
    return in;
}

DWORD downFlag(int button)
{
    switch (button) {
    case MKBTN_RIGHT: return MOUSEEVENTF_RIGHTDOWN;
    case MKBTN_MIDDLE: return MOUSEEVENTF_MIDDLEDOWN;
    default: return MOUSEEVENTF_LEFTDOWN;
    }
}

DWORD upFlag(int button)
{
    switch (button) {
    case MKBTN_RIGHT: return MOUSEEVENTF_RIGHTUP;
    case MKBTN_MIDDLE: return MOUSEEVENTF_MIDDLEUP;
    default: return MOUSEEVENTF_LEFTUP;
    }
}

} // namespace

namespace inject {

void move(int dx, int dy)
{
    if (dx == 0 && dy == 0)
        return;
    INPUT in = makeMouse(MOUSEEVENTF_MOVE, dx, dy, 0);
    SendInput(1, &in, sizeof(INPUT));
}

void wheel(int detents)
{
    if (detents == 0)
        return;
    INPUT in = makeMouse(MOUSEEVENTF_WHEEL, 0, 0, detents * WHEEL_DELTA);
    SendInput(1, &in, sizeof(INPUT));
}

void buttonDown(int button)
{
    INPUT in = makeMouse(downFlag(button), 0, 0, 0);
    SendInput(1, &in, sizeof(INPUT));
}

void buttonUp(int button)
{
    INPUT in = makeMouse(upFlag(button), 0, 0, 0);
    SendInput(1, &in, sizeof(INPUT));
}

void click(int button)
{
    buttonDown(button);
    buttonUp(button);
}

void releaseAllButtons()
{
    INPUT ins[3] = {};
    ins[0] = makeMouse(MOUSEEVENTF_LEFTUP, 0, 0, 0);
    ins[1] = makeMouse(MOUSEEVENTF_RIGHTUP, 0, 0, 0);
    ins[2] = makeMouse(MOUSEEVENTF_MIDDLEUP, 0, 0, 0);
    SendInput(3, ins, sizeof(INPUT));
}

} // namespace inject
