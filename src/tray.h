#pragma once

#include <windows.h>

class TrayIcon {
public:
    bool create(HWND owner, HINSTANCE inst, UINT callbackMsg);
    void update(bool enabled);
    void remove();

private:
    NOTIFYICONDATAW m_nid = {};
    bool m_added = false;
};
