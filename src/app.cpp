#include "app.h"

#include "engine.h"
#include "hook.h"
#include "inject.h"
#include "numlock.h"
#include "resource.h"
#include "tray.h"

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

#include <d3d11.h>
#include <dxgi.h>

#include <shellapi.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

const wchar_t* const kMouseKeyWndClass = L"MouseKeyWnd";

namespace {

UINT g_showSettingsMsg = 0;

bool bindingsEqual(const Binding* a, const Binding* b)
{
    for (int i = 0; i < kActionCount; ++i)
        if (a[i].scan != b[i].scan || a[i].action != b[i].action)
            return false;
    return true;
}

void copyBindings(Binding* dst, const Binding* src)
{
    for (int i = 0; i < kActionCount; ++i)
        dst[i] = src[i];
}

std::wstring exeDir()
{
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring dir(path);
    const size_t slash = dir.find_last_of(L'\\');
    if (slash != std::wstring::npos)
        dir.resize(slash + 1);
    return dir;
}

} // namespace

App::App()
    : m_inst(nullptr)
    , m_hwnd(nullptr)
    , m_cfg()
    , m_iniPath()
    , m_iniPathUtf8Buf()
    , m_engine(nullptr)
    , m_hook(nullptr)
    , m_tray(nullptr)
    , m_uiState(nullptr)
    , m_lastBindings()
    , m_quit(false)
    , m_d3dDevice(nullptr)
    , m_d3dContext(nullptr)
    , m_swap(nullptr)
    , m_rtv(nullptr)
{
}

App& App::get()
{
    static App instance;
    return instance;
}

bool App::createWindow()
{
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wndProc;
    wc.hInstance = m_inst;
    wc.hIcon = (HICON)LoadImageW(m_inst, MAKEINTRESOURCEW(IDI_MK_ON),
                                 IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = kMouseKeyWndClass;
    if (!RegisterClassExW(&wc))
        return false;

    RECT rect = { 0, 0, 540, 600 };
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    m_hwnd = CreateWindowExW(0, kMouseKeyWndClass, L"鼠标键",
                             WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                             rect.right - rect.left, rect.bottom - rect.top,
                             nullptr, nullptr, m_inst, nullptr);
    return m_hwnd != nullptr;
}

bool App::initD3D()
{
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = m_hwnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, ARRAYSIZE(levels),
        D3D11_SDK_VERSION, &sd, &m_swap, &m_d3dDevice, nullptr, &m_d3dContext);
    if (FAILED(hr))
        return false;
    createRenderTarget();
    return true;
}

void App::createRenderTarget()
{
    ID3D11Texture2D* back = nullptr;
    if (SUCCEEDED(m_swap->GetBuffer(0, IID_PPV_ARGS(&back)))) {
        m_d3dDevice->CreateRenderTargetView(back, nullptr, &m_rtv);
        back->Release();
    }
}

void App::releaseRenderTarget()
{
    if (m_rtv) {
        m_rtv->Release();
        m_rtv = nullptr;
    }
}

void App::shutdownD3D()
{
    releaseRenderTarget();
    if (m_swap) {
        m_swap->Release();
        m_swap = nullptr;
    }
    if (m_d3dContext) {
        m_d3dContext->Release();
        m_d3dContext = nullptr;
    }
    if (m_d3dDevice) {
        m_d3dDevice->Release();
        m_d3dDevice = nullptr;
    }
}

bool App::initImGui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    struct FontCandidate {
        const wchar_t* path;
    };
    const wchar_t* candidates[] = {
        L"C:\\Windows\\Fonts\\msyh.ttc",
        L"C:\\Windows\\Fonts\\deng.ttf",
        L"C:\\Windows\\Fonts\\simhei.ttf",
        L"C:\\Windows\\Fonts\\simsun.ttc",
    };
    for (const wchar_t* c : candidates) {
        const DWORD attr = GetFileAttributesW(c);
        if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY))
            continue;
        char utf8[MAX_PATH * 3];
        WideCharToMultiByte(CP_UTF8, 0, c, -1, utf8, sizeof(utf8), nullptr, nullptr);
        if (io.Fonts->AddFontFromFileTTF(utf8, 18.0f, nullptr,
                                         io.Fonts->GetGlyphRangesChineseSimplifiedCommon()))
            break;
    }

    ImGui::StyleColorsDark();
    if (!ImGui_ImplWin32_Init(m_hwnd))
        return false;
    return ImGui_ImplDX11_Init(m_d3dDevice, m_d3dContext);
}

void App::renderFrame()
{
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    ui::draw(*this, *m_uiState);
    ImGui::Render();

    const float clear[4] = { 0.07f, 0.07f, 0.08f, 1.0f };
    m_d3dContext->OMSetRenderTargets(1, &m_rtv, nullptr);
    m_d3dContext->ClearRenderTargetView(m_rtv, clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    m_swap->Present(1, 0);
}

int App::run(HINSTANCE inst)
{
    m_inst = inst;
    g_showSettingsMsg = RegisterWindowMessageW(L"MouseKeyShowSettings");

    m_iniPath = exeDir() + L"mousekey.ini";
    loadConfig(m_iniPath.c_str(), m_cfg);
    m_uiState = new ui::UiState();

    if (!createWindow())
        return 1;
    if (!initD3D()) {
        shutdownD3D();
        DestroyWindow(m_hwnd);
        return 1;
    }
    initImGui();

    m_engine = new MotionEngine();
    m_engine->start(m_cfg.motion, m_cfg.csvLog);

    if (m_cfg.fixNumLock)
        numlock::turnOff();

    m_hook = new KeyboardHook();
    m_hook->setEngine(m_engine);
    m_hook->install(m_hwnd, WM_APP_CAPTURE);
    m_hook->setBindings(m_cfg.bindings, kActionCount);
    m_hook->setEnabled(m_cfg.enabled);
    copyBindings(m_lastBindings, m_cfg.bindings);

    m_tray = new TrayIcon();
    m_tray->create(m_hwnd, m_inst, WM_APP_TRAY);
    m_tray->update(m_cfg.enabled);

    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);

    MSG msg;
    for (;;) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                m_quit = true;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (m_quit)
            break;

        applyLive();

        if (IsWindowVisible(m_hwnd)) {
            renderFrame();
        } else if (!PeekMessageW(&msg, nullptr, 0, 0, PM_NOREMOVE)) {
            WaitMessage();
        }
    }

    saveNow();
    m_tray->remove();
    m_hook->remove();
    m_engine->stop();
    if (!m_cfg.enabled)
        inject::releaseAllButtons();

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    shutdownD3D();
    if (IsWindow(m_hwnd))
        DestroyWindow(m_hwnd);

    delete m_tray;
    delete m_hook;
    delete m_engine;
    delete m_uiState;
    m_tray = nullptr;
    m_hook = nullptr;
    m_engine = nullptr;
    m_uiState = nullptr;
    return 0;
}

void App::applyLive()
{
    if (m_engine)
        m_engine->setParams(m_cfg.motion);
    if (m_hook && !bindingsEqual(m_lastBindings, m_cfg.bindings)) {
        m_hook->setBindings(m_cfg.bindings, kActionCount);
        copyBindings(m_lastBindings, m_cfg.bindings);
    }
}

void App::saveNow()
{
    saveConfig(m_iniPath.c_str(), m_cfg);
    if (m_uiState)
        m_uiState->savedFlash = 1.2f;
}

const char* App::iniPathUtf8()
{
    WideCharToMultiByte(CP_UTF8, 0, m_iniPath.c_str(), -1,
                        m_iniPathUtf8Buf, sizeof(m_iniPathUtf8Buf), nullptr, nullptr);
    return m_iniPathUtf8Buf;
}

void App::setEnabled(bool on)
{
    if (m_cfg.enabled == on)
        return;
    m_cfg.enabled = on;
    if (!on) {
        m_engine->setInput(0);
        inject::releaseAllButtons();
    }
    m_hook->setEnabled(on);
    m_tray->update(on);
    saveNow();
}

void App::showSettings()
{
    ShowWindow(m_hwnd, SW_SHOW);
    SetForegroundWindow(m_hwnd);
    if (IsIconic(m_hwnd))
        ShowWindow(m_hwnd, SW_RESTORE);
}

void App::onCaptureResult(unsigned short scan, bool cancelled)
{
    const int act = m_uiState->captureAction;
    m_uiState->captureAction = 0;
    m_hook->setCaptureMode(false);
    if (cancelled || scan == 0 || act == 0)
        return;

    for (int i = 1; i < kActionCount; ++i) {
        if (i != act && m_cfg.bindings[i].scan == scan) {
            m_uiState->conflictAction = i;
            m_uiState->conflictScan = scan;
            return;
        }
    }
    m_cfg.bindings[act].scan = scan;
    m_uiState->conflictAction = 0;
    applyLive();
    saveNow();
}

void App::quit()
{
    m_quit = true;
    if (IsWindow(m_hwnd))
        DestroyWindow(m_hwnd);
}

void App::handleTray(LPARAM lp)
{
    switch (LOWORD(lp)) {
    case WM_LBUTTONUP:
        showSettings();
        break;
    case WM_RBUTTONUP:
    case WM_CONTEXTMENU:
        showTrayMenu();
        break;
    default:
        break;
    }
}

void App::showTrayMenu()
{
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING | (m_cfg.enabled ? MF_CHECKED : 0),
               IDM_TOGGLE, m_cfg.enabled ? L"停用" : L"启用");
    AppendMenuW(menu, MF_STRING, IDM_SETTINGS, L"设置…");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, IDM_EXIT, L"退出");

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(m_hwnd);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, m_hwnd, nullptr);
    PostMessageW(m_hwnd, WM_NULL, 0, 0);
    DestroyMenu(menu);
}

LRESULT CALLBACK App::wndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    if (ImGui_ImplWin32_WndProcHandler(h, msg, wp, lp))
        return 1;

    App& app = App::get();
    switch (msg) {
    case WM_APP_TRAY:
        app.handleTray(lp);
        return 0;
    case WM_APP_CAPTURE:
        app.onCaptureResult((unsigned short)wp, lp != 0);
        return 0;
    case WM_GETMINMAXINFO: {
        auto* mmi = reinterpret_cast<MINMAXINFO*>(lp);
        mmi->ptMinTrackSize.x = 480;
        mmi->ptMinTrackSize.y = 420;
        return 0;
    }
    case WM_SIZE: {
        if (wp == SIZE_MINIMIZED)
            return 0;
        if (app.m_swap && LOWORD(lp) && HIWORD(lp)) {
            app.releaseRenderTarget();
            app.m_swap->ResizeBuffers(0, LOWORD(lp), HIWORD(lp),
                                      DXGI_FORMAT_UNKNOWN, 0);
            app.createRenderTarget();
        }
        return 0;
    }
    case WM_COMMAND: {
        switch (LOWORD(wp)) {
        case IDM_TOGGLE:
            app.setEnabled(!app.m_cfg.enabled);
            return 0;
        case IDM_SETTINGS:
            app.showSettings();
            return 0;
        case IDM_EXIT:
            app.quit();
            return 0;
        default:
            return 0;
        }
    }
    case WM_CLOSE:
        ShowWindow(h, SW_HIDE);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        if (msg == g_showSettingsMsg) {
            app.showSettings();
            return 0;
        }
        return DefWindowProcW(h, msg, wp, lp);
    }
}
