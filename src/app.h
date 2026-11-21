#pragma once

#include <windows.h>

#include "config.h"
#include "messages.h"
#include "ui.h"

#include <string>

class MotionEngine;
class KeyboardHook;
class TrayIcon;
struct ID3D11Device;
struct ID3D11DeviceContext;
struct IDXGISwapChain;
struct ID3D11RenderTargetView;

extern const wchar_t* const kMouseKeyWndClass;

class App {
public:
    static App& get();

    int run(HINSTANCE inst);

    Config& cfg() { return m_cfg; }
    MotionEngine& engine() { return *m_engine; }
    KeyboardHook& hook() { return *m_hook; }
    HWND hwnd() const { return m_hwnd; }
    ui::UiState& uiState() { return *m_uiState; }
    const char* iniPathUtf8();

    void setEnabled(bool on);
    void applyNumLockOff();
    void restoreNumLock();
    bool autostartEnabled() const { return m_autostart; }
    bool setAutostart(bool on);
    void showSettings();
    void onCaptureResult(unsigned short scan, bool cancelled);
    void applyLive();
    void saveNow();
    void quit();

private:
    App();

    bool createWindow();
    bool initD3D();
    void createRenderTarget();
    void releaseRenderTarget();
    void shutdownD3D();
    bool initImGui();
    void renderFrame();
    void handleTray(LPARAM lp);
    void showTrayMenu();

    static LRESULT CALLBACK wndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp);

    HINSTANCE m_inst;
    HWND m_hwnd;
    Config m_cfg;
    std::wstring m_iniPath;
    char m_iniPathUtf8Buf[MAX_PATH * 3];

    MotionEngine* m_engine;
    KeyboardHook* m_hook;
    TrayIcon* m_tray;
    ui::UiState* m_uiState;
    Binding m_lastBindings[kActionCount];
    bool m_autostart;
    bool m_quit;

    ID3D11Device* m_d3dDevice;
    ID3D11DeviceContext* m_d3dContext;
    IDXGISwapChain* m_swap;
    ID3D11RenderTargetView* m_rtv;
};
