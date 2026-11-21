#include "inject.h"

#include "buttons.h"

#include <windows.h>

namespace {

ButtonState g_buttons;
SRWLOCK g_lock = SRWLOCK_INIT;

INPUT makeMouse(DWORD flags, int dx, int dy, int data)
{
    INPUT in = {};
    in.type = INPUT_MOUSE;
    in.mi.dx = dx;
    in.mi.dy = dy;
    in.mi.mouseData = (DWORD)(ULONG_PTR)(DWORD)data;
    in.mi.dwFlags = flags;
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
    AcquireSRWLockExclusive(&g_lock);
    if (g_buttons.press(button)) {
        INPUT in = makeMouse(downFlag(button), 0, 0, 0);
        SendInput(1, &in, sizeof(INPUT));
    }
    ReleaseSRWLockExclusive(&g_lock);
}

void buttonUp(int button)
{
    AcquireSRWLockExclusive(&g_lock);
    if (g_buttons.release(button)) {
        INPUT in = makeMouse(upFlag(button), 0, 0, 0);
        SendInput(1, &in, sizeof(INPUT));
    }
    ReleaseSRWLockExclusive(&g_lock);
}

void click(int button)
{
    buttonDown(button);
    buttonUp(button);
}

void releaseAllButtons()
{
    AcquireSRWLockExclusive(&g_lock);
    const int held = g_buttons.takeAll();
    INPUT ins[ButtonState::kMaxButtons];
    int count = 0;
    for (int b = 0; b < ButtonState::kMaxButtons; ++b) {
        if (held & (1 << b))
            ins[count++] = makeMouse(upFlag(b), 0, 0, 0);
    }
    if (count > 0)
        SendInput((UINT)count, ins, sizeof(INPUT));
    ReleaseSRWLockExclusive(&g_lock);
}

} // namespace inject
