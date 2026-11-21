#include "tray.h"
#include "resource.h"

#include <shellapi.h>

bool TrayIcon::create(HWND owner, HINSTANCE inst, UINT callbackMsg)
{
    m_nid = {};
    m_nid.cbSize = sizeof(m_nid);
    m_nid.hWnd = owner;
    m_nid.uID = 1;
    m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    m_nid.uCallbackMessage = callbackMsg;
    m_nid.hIcon = (HICON)LoadImageW(inst, MAKEINTRESOURCEW(IDI_MK_OFF),
                                    IMAGE_ICON, 0, 0,
                                    LR_DEFAULTSIZE | LR_SHARED);
    lstrcpynW(m_nid.szTip, L"鼠标键（未启用）",
              (int)(sizeof(m_nid.szTip) / sizeof(m_nid.szTip[0])));
    m_added = (Shell_NotifyIconW(NIM_ADD, &m_nid) != FALSE);
    return m_added;
}

void TrayIcon::update(bool enabled)
{
    if (!m_added)
        return;
    HINSTANCE inst = (HINSTANCE)GetWindowLongPtrW(m_nid.hWnd, GWLP_HINSTANCE);
    m_nid.hIcon = (HICON)LoadImageW(inst,
                                    MAKEINTRESOURCEW(enabled ? IDI_MK_ON : IDI_MK_OFF),
                                    IMAGE_ICON, 0, 0,
                                    LR_DEFAULTSIZE | LR_SHARED);
    if (enabled)
        lstrcpynW(m_nid.szTip, L"鼠标键（已启用）",
                  (int)(sizeof(m_nid.szTip) / sizeof(m_nid.szTip[0])));
    else
        lstrcpynW(m_nid.szTip, L"鼠标键（未启用）",
                  (int)(sizeof(m_nid.szTip) / sizeof(m_nid.szTip[0])));
    Shell_NotifyIconW(NIM_MODIFY, &m_nid);
}

void TrayIcon::remove()
{
    if (!m_added)
        return;
    Shell_NotifyIconW(NIM_DELETE, &m_nid);
    m_added = false;
}
