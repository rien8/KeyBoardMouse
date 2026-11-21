#pragma once

#include "config.h"

#include <windows.h>
#include <atomic>

struct HookEvent {
    unsigned short scan;
    bool extended;
    bool isUp;
};

struct KeyOutcome {
    bool swallow;
    Action action;
    bool repeat;
    bool captured;
    unsigned short capturedScan;
    bool cancelled;
};

class KeyState {
public:
    KeyState();

    void setBindings(const Binding* bindings, int count);
    void setCaptureMode(bool on);
    bool captureMode() const;

    KeyOutcome onKey(const HookEvent& ev);

    unsigned directionMask() const;
    unsigned wheelMask() const;

private:
    bool keyIsDown(unsigned short scanWithExt) const;
    void recomputeBits();

    Binding m_bindings[kActionCount];
    bool m_capture;
    unsigned short m_down[16];
    int m_downCount;
    unsigned m_dirBits;
    unsigned m_wheelBits;
};

class MotionEngine;

enum ClickButton {
    CLICK_BTN_LEFT = 0,
    CLICK_BTN_RIGHT = 1,
};

class KeyboardHook {
public:
    KeyboardHook();
    ~KeyboardHook();
    KeyboardHook(const KeyboardHook&) = delete;
    KeyboardHook& operator=(const KeyboardHook&) = delete;

    bool install(HWND notifyWnd, UINT captureMsg);
    void remove();

    void setEngine(MotionEngine* engine);
    void setBindings(const Binding* bindings, int count);
    void setCaptureMode(bool on);
    void setEnabled(bool on);
    int clickButton() const;
    bool numLockOn() const;
    void pressNumLockToggle();

private:
    static DWORD WINAPI threadProc(LPVOID self);
    static LRESULT CALLBACK hookProc(int code, WPARAM wp, LPARAM lp);
    void applySideEffects(const KeyOutcome& o, bool isUp);

    HWND m_notifyWnd;
    UINT m_captureMsg;
    MotionEngine* m_engine;
    HHOOK m_hook;
    DWORD m_threadId;
    HANDLE m_thread;
    HANDLE m_readyEvent;
    bool m_installed;
    std::atomic<bool> m_enabled;
    std::atomic<bool> m_captureReq;
    std::atomic<bool> m_bindingsDirty;
    std::atomic<int> m_clickButton;
    std::atomic<bool> m_numLockOn;
    std::atomic<int> m_injectedNumLock;
    int m_heldButton;
    SRWLOCK m_bindingLock;
    Binding m_pending[kActionCount];
    KeyState m_state;

    static KeyboardHook* s_instance;
};
