#include "hook.h"

#include "engine.h"
#include "inject.h"

namespace {

unsigned directionBitsFor(Action a)
{
    switch (a) {
    case Action::Up: return DIR_UP;
    case Action::Down: return DIR_DOWN;
    case Action::Left: return DIR_LEFT;
    case Action::Right: return DIR_RIGHT;
    case Action::UpLeft: return DIR_UP | DIR_LEFT;
    case Action::UpRight: return DIR_UP | DIR_RIGHT;
    case Action::DownLeft: return DIR_DOWN | DIR_LEFT;
    case Action::DownRight: return DIR_DOWN | DIR_RIGHT;
    default: return 0;
    }
}

unsigned wheelBitsFor(Action a)
{
    switch (a) {
    case Action::WheelUp: return WHEEL_UP;
    case Action::WheelDown: return WHEEL_DOWN;
    default: return 0;
    }
}

Action actionForScan(const Binding* bindings, unsigned short scanWithExt)
{
    for (int i = 0; i < kActionCount; ++i)
        if (bindings[i].scan == scanWithExt && bindings[i].action != Action::None)
            return bindings[i].action;
    return Action::None;
}

} // namespace

KeyState::KeyState()
    : m_capture(false)
    , m_downCount(0)
    , m_dirBits(0)
    , m_wheelBits(0)
{
    for (int i = 0; i < kActionCount; ++i)
        m_bindings[i] = Binding{ 0, static_cast<Action>(i) };
}

void KeyState::setBindings(const Binding* bindings, int count)
{
    for (int i = 0; i < count && i < kActionCount; ++i)
        m_bindings[i] = bindings[i];
    recomputeBits();
}

void KeyState::setCaptureMode(bool on)
{
    m_capture = on;
}

bool KeyState::captureMode() const
{
    return m_capture;
}

unsigned KeyState::directionMask() const
{
    return m_dirBits;
}

unsigned KeyState::wheelMask() const
{
    return m_wheelBits;
}

bool KeyState::keyIsDown(unsigned short scanWithExt) const
{
    for (int i = 0; i < m_downCount; ++i)
        if (m_down[i] == scanWithExt)
            return true;
    return false;
}

void KeyState::recomputeBits()
{
    unsigned dir = 0;
    unsigned wheel = 0;
    for (int i = 0; i < m_downCount; ++i) {
        const Action a = actionForScan(m_bindings, m_down[i]);
        dir |= directionBitsFor(a);
        wheel |= wheelBitsFor(a);
    }
    m_dirBits = dir;
    m_wheelBits = wheel;
}

KeyOutcome KeyState::onKey(const HookEvent& ev)
{
    KeyOutcome o = {};
    o.action = Action::None;

    const unsigned short code = ev.scan | (ev.extended ? SCAN_EXT : 0);

    if (m_capture) {
        o.swallow = true;
        if (ev.isUp)
            return o;
        if (code == 0x01) {
            o.cancelled = true;
            m_capture = false;
            return o;
        }
        o.captured = true;
        o.capturedScan = code;
        m_capture = false;
        return o;
    }

    const Action act = actionForScan(m_bindings, code);
    o.action = act;
    if (act == Action::None) {
        o.swallow = false;
        return o;
    }

    o.swallow = true;
    if (ev.isUp) {
        for (int i = 0; i < m_downCount; ++i) {
            if (m_down[i] == code) {
                m_down[i] = m_down[m_downCount - 1];
                --m_downCount;
                break;
            }
        }
        recomputeBits();
        return o;
    }

    if (keyIsDown(code)) {
        o.repeat = true;
        o.action = Action::None;
        return o;
    }

    if (m_downCount < (int)(sizeof(m_down) / sizeof(m_down[0])))
        m_down[m_downCount++] = code;
    recomputeBits();
    return o;
}

KeyboardHook* KeyboardHook::s_instance = nullptr;

KeyboardHook::KeyboardHook()
    : m_notifyWnd(nullptr)
    , m_captureMsg(0)
    , m_engine(nullptr)
    , m_hook(nullptr)
    , m_threadId(0)
    , m_thread(nullptr)
    , m_readyEvent(nullptr)
    , m_installed(false)
    , m_enabled(false)
    , m_captureReq(false)
    , m_bindingsDirty(false)
    , m_clickButton(CLICK_BTN_LEFT)
    , m_heldButton(-1)
    , m_bindingLock(SRWLOCK_INIT)
    , m_state()
{
    for (int i = 0; i < kActionCount; ++i)
        m_pending[i] = Binding{ 0, static_cast<Action>(i) };
}

KeyboardHook::~KeyboardHook()
{
    remove();
}

bool KeyboardHook::install(HWND notifyWnd, UINT captureMsg)
{
    m_notifyWnd = notifyWnd;
    m_captureMsg = captureMsg;
    m_readyEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    s_instance = this;
    m_thread = CreateThread(nullptr, 0, threadProc, this, 0, &m_threadId);
    if (!m_thread)
        return false;
    WaitForSingleObject(m_readyEvent, 5000);
    CloseHandle(m_readyEvent);
    m_readyEvent = nullptr;
    return m_installed;
}

void KeyboardHook::remove()
{
    if (m_thread) {
        PostThreadMessageW(m_threadId, WM_QUIT, 0, 0);
        WaitForSingleObject(m_thread, 5000);
        CloseHandle(m_thread);
        m_thread = nullptr;
        m_threadId = 0;
    }
    if (s_instance == this)
        s_instance = nullptr;
}

void KeyboardHook::setEngine(MotionEngine* engine)
{
    m_engine = engine;
}

void KeyboardHook::setBindings(const Binding* bindings, int count)
{
    AcquireSRWLockExclusive(&m_bindingLock);
    for (int i = 0; i < count && i < kActionCount; ++i)
        m_pending[i] = bindings[i];
    ReleaseSRWLockExclusive(&m_bindingLock);
    m_bindingsDirty.store(true, std::memory_order_relaxed);
}

void KeyboardHook::setCaptureMode(bool on)
{
    m_captureReq.store(on, std::memory_order_relaxed);
}

void KeyboardHook::setEnabled(bool on)
{
    m_enabled.store(on, std::memory_order_relaxed);
}

DWORD WINAPI KeyboardHook::threadProc(LPVOID self)
{
    auto* hook = static_cast<KeyboardHook*>(self);
    hook->m_hook = SetWindowsHookExW(WH_KEYBOARD_LL, hookProc, nullptr, 0);
    hook->m_installed = (hook->m_hook != nullptr);
    if (hook->m_readyEvent)
        SetEvent(hook->m_readyEvent);
    if (!hook->m_hook)
        return 1;

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
    }
    UnhookWindowsHookEx(hook->m_hook);
    hook->m_hook = nullptr;
    return 0;
}

namespace {

} // namespace

void KeyboardHook::applySideEffects(const KeyOutcome& o, bool isUp)
{
    if (o.repeat)
        return;

    const bool rightMode = (m_clickButton.load(std::memory_order_relaxed) == CLICK_BTN_RIGHT);
    const int activeButton = rightMode ? MKBTN_RIGHT : MKBTN_LEFT;

    if (isUp) {
        if (o.action == Action::Hold && m_heldButton >= 0) {
            inject::buttonUp(m_heldButton);
            m_heldButton = -1;
        }
        return;
    }

    switch (o.action) {
    case Action::ModeLeft:
        m_clickButton.store(CLICK_BTN_LEFT, std::memory_order_relaxed);
        break;
    case Action::ModeRight:
        m_clickButton.store(CLICK_BTN_RIGHT, std::memory_order_relaxed);
        break;
    case Action::Click:
        inject::click(activeButton);
        break;
    case Action::RightClick:
        inject::click(MKBTN_RIGHT);
        break;
    case Action::MiddleClick:
        inject::click(MKBTN_MIDDLE);
        break;
    case Action::Hold:
        m_heldButton = activeButton;
        inject::buttonDown(activeButton);
        break;
    case Action::Release:
        inject::releaseAllButtons();
        m_heldButton = -1;
        break;
    default:
        break;
    }
}

int KeyboardHook::clickButton() const
{
    return m_clickButton.load(std::memory_order_relaxed);
}

LRESULT CALLBACK KeyboardHook::hookProc(int code, WPARAM wp, LPARAM lp)
{
    if (code == HC_ACTION) {
        KeyboardHook* self = s_instance;
        const auto* kh = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lp);

        HookEvent ev;
        ev.scan = (unsigned short)(kh->scanCode & 0xFF);
        ev.extended = (kh->flags & LLKHF_EXTENDED) != 0;
        ev.isUp = (wp == WM_KEYUP || wp == WM_SYSKEYUP);

        if (self->m_bindingsDirty.exchange(false, std::memory_order_relaxed)) {
            AcquireSRWLockExclusive(&self->m_bindingLock);
            self->m_state.setBindings(self->m_pending, kActionCount);
            ReleaseSRWLockExclusive(&self->m_bindingLock);
        }
        const bool capReq = self->m_captureReq.load(std::memory_order_relaxed);
        if (self->m_state.captureMode() != capReq)
            self->m_state.setCaptureMode(capReq);

        const KeyOutcome o = self->m_state.onKey(ev);

        if (o.captured || o.cancelled) {
            PostMessageW(self->m_notifyWnd, self->m_captureMsg,
                         (WPARAM)o.capturedScan, o.cancelled ? 1 : 0);
            return 1;
        }

        const bool enabled = self->m_enabled.load(std::memory_order_relaxed);
        if (enabled) {
            self->applySideEffects(o, ev.isUp);
            if (self->m_engine)
                self->m_engine->setInput(
                    self->m_state.directionMask() | self->m_state.wheelMask());
            if (o.swallow)
                return 1;
        } else if (self->m_engine) {
            self->m_engine->setInput(0);
        }
    }
    return CallNextHookEx(nullptr, code, wp, lp);
}
