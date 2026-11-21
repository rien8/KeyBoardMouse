#include "config.h"

#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

MotionParams defaultMotionParams()
{
    MotionParams p;
    p.vStart = 40.f;
    p.vMax = 1600.f;
    p.rampMs = 500.f;
    p.gamma = 1.0f;
    p.diagScale = 0.7071f;
    p.wheelRate = 12.f;
    return p;
}

Config defaultConfig()
{
    Config c = {};
    c.motion = defaultMotionParams();
    for (int i = 0; i < kActionCount; ++i)
        c.bindings[i] = Binding{ 0, static_cast<Action>(i) };

    c.bindings[(int)Action::Up].scan = 0x48;
    c.bindings[(int)Action::Down].scan = 0x50;
    c.bindings[(int)Action::Left].scan = 0x4B;
    c.bindings[(int)Action::Right].scan = 0x4D;
    c.bindings[(int)Action::UpLeft].scan = 0x47;
    c.bindings[(int)Action::UpRight].scan = 0x49;
    c.bindings[(int)Action::DownLeft].scan = 0x4F;
    c.bindings[(int)Action::DownRight].scan = 0x51;
    c.bindings[(int)Action::Click].scan = 0x4C;
    c.bindings[(int)Action::RightClick].scan = 0x53;
    c.bindings[(int)Action::Hold].scan = 0x52;
    c.bindings[(int)Action::WheelUp].scan = 0x4E;
    c.bindings[(int)Action::WheelDown].scan = 0x4A;
    c.bindings[(int)Action::ModeLeft].scan = 0x135;
    c.bindings[(int)Action::ModeRight].scan = 0x37;
    c.bindings[(int)Action::ToggleEnabled].scan = 0x45;

    c.enabled = true;
    c.csvLog = false;
    c.fixNumLock = true;
    return c;
}

namespace {

struct FloatParam {
    const wchar_t* key;
    float MotionParams::*field;
    float def;
    float lo;
    float hi;
};

bool readFloat(const wchar_t* path, const wchar_t* section, const wchar_t* key,
               float def, float lo, float hi, float& out)
{
    wchar_t buf[64];
    GetPrivateProfileStringW(section, key, L"", buf, 64, path);
    if (buf[0] == L'\0') {
        out = def;
        return false;
    }
    wchar_t* end = nullptr;
    double v = wcstod(buf, &end);
    if (end == buf || v < (double)lo || v > (double)hi) {
        out = def;
        return false;
    }
    out = (float)v;
    return true;
}

bool readScan(const wchar_t* path, const wchar_t* section, const wchar_t* key,
              unsigned short def, unsigned short& out)
{
    wchar_t buf[64];
    GetPrivateProfileStringW(section, key, L"", buf, 64, path);
    if (buf[0] == L'\0') {
        out = def;
        return false;
    }
    wchar_t* end = nullptr;
    long v = wcstol(buf, &end, 0);
    if (end == buf || v < 0 || v >= 0x200) {
        out = def;
        return false;
    }
    out = (unsigned short)v;
    return true;
}

bool readBool(const wchar_t* path, const wchar_t* section, const wchar_t* key,
              bool def, bool& out)
{
    wchar_t buf[16];
    GetPrivateProfileStringW(section, key, L"", buf, 16, path);
    if (buf[0] == L'\0') {
        out = def;
        return false;
    }
    out = (buf[0] != L'0');
    return true;
}

void writeFloat(const wchar_t* path, const wchar_t* section, const wchar_t* key, float v)
{
    wchar_t buf[64];
    swprintf(buf, 64, L"%g", (double)v);
    WritePrivateProfileStringW(section, key, buf, path);
}

void writeScan(const wchar_t* path, const wchar_t* section, const wchar_t* key, unsigned short v)
{
    wchar_t buf[32];
    swprintf(buf, 32, L"0x%X", (unsigned)v);
    WritePrivateProfileStringW(section, key, buf, path);
}

void writeBool(const wchar_t* path, const wchar_t* section, const wchar_t* key, bool v)
{
    WritePrivateProfileStringW(section, key, v ? L"1" : L"0", path);
}

const wchar_t* actionKey(int action)
{
    static const wchar_t* names[kActionCount] = {
        L"None",
        L"Up", L"Down", L"Left", L"Right",
        L"UpLeft", L"UpRight", L"DownLeft", L"DownRight",
        L"Click", L"RightClick", L"MiddleClick",
        L"Hold", L"Release",
        L"WheelUp", L"WheelDown",
        L"ModeLeft", L"ModeRight",
        L"ToggleEnabled",
    };
    return names[action];
}

struct ScanName {
    unsigned short scan;
    const char* name;
};

const ScanName g_scanNames[] = {
    { 0x01, "Esc" },
    { 0x02, "1" }, { 0x03, "2" }, { 0x04, "3" }, { 0x05, "4" }, { 0x06, "5" },
    { 0x07, "6" }, { 0x08, "7" }, { 0x09, "8" }, { 0x0A, "9" }, { 0x0B, "0" },
    { 0x0C, "-" }, { 0x0D, "=" },
    { 0x0E, "Backspace" }, { 0x0F, "Tab" },
    { 0x10, "Q" }, { 0x11, "W" }, { 0x12, "E" }, { 0x13, "R" }, { 0x14, "T" },
    { 0x15, "Y" }, { 0x16, "U" }, { 0x17, "I" }, { 0x18, "O" }, { 0x19, "P" },
    { 0x1A, "[" }, { 0x1B, "]" },
    { 0x1C, "Enter" }, { 0x1D, "Ctrl" },
    { 0x1E, "A" }, { 0x1F, "S" }, { 0x20, "D" }, { 0x21, "F" }, { 0x22, "G" },
    { 0x23, "H" }, { 0x24, "J" }, { 0x25, "K" }, { 0x26, "L" },
    { 0x27, ";" }, { 0x28, "'" }, { 0x29, "`" },
    { 0x2A, "Shift" }, { 0x2B, "\\" },
    { 0x2C, "Z" }, { 0x2D, "X" }, { 0x2E, "C" }, { 0x2F, "V" },
    { 0x30, "B" }, { 0x31, "N" }, { 0x32, "M" },
    { 0x33, "," }, { 0x34, "." }, { 0x35, "/" },
    { 0x36, "Shift" },
    { 0x37, u8"小键盘 *" }, { 0x38, "Alt" }, { 0x39, "Space" },
    { 0x3A, "CapsLock" },
    { 0x3B, "F1" }, { 0x3C, "F2" }, { 0x3D, "F3" }, { 0x3E, "F4" }, { 0x3F, "F5" },
    { 0x40, "F6" }, { 0x41, "F7" }, { 0x42, "F8" }, { 0x43, "F9" }, { 0x44, "F10" },
    { 0x45, "NumLock" }, { 0x46, "ScrollLock" },
    { 0x47, u8"小键盘 7" }, { 0x48, u8"小键盘 8" }, { 0x49, u8"小键盘 9" },
    { 0x4A, u8"小键盘 -" },
    { 0x4B, u8"小键盘 4" }, { 0x4C, u8"小键盘 5" }, { 0x4D, u8"小键盘 6" },
    { 0x4E, u8"小键盘 +" },
    { 0x4F, u8"小键盘 1" }, { 0x50, u8"小键盘 2" }, { 0x51, u8"小键盘 3" },
    { 0x52, u8"小键盘 0" }, { 0x53, u8"小键盘 ." },
    { 0x54, "PrtSc" }, { 0x57, "F11" }, { 0x58, "F12" },
    { 0x11C, u8"小键盘 Enter" }, { 0x11D, "Ctrl" }, { 0x135, u8"小键盘 /" },
    { 0x137, "PrtSc" }, { 0x138, "Alt" },
    { 0x147, "Home" }, { 0x148, u8"↑" }, { 0x149, "PgUp" },
    { 0x14B, u8"←" }, { 0x14D, u8"→" },
    { 0x14F, "End" }, { 0x150, u8"↓" }, { 0x151, "PgDn" },
    { 0x152, "Ins" }, { 0x153, "Del" },
    { 0x15B, u8"左 Win" }, { 0x15C, u8"右 Win" }, { 0x15D, "Menu" },
    { 0x5B, u8"左 Win" }, { 0x5C, u8"右 Win" },
};

const char* lookupScanName(unsigned short scan)
{
    for (const auto& e : g_scanNames)
        if (e.scan == scan)
            return e.name;
    return nullptr;
}

const char* g_actionNames[kActionCount] = {
    "",
    u8"上", u8"下", u8"左", u8"右",
    u8"左上", u8"右上", u8"左下", u8"右下",
    u8"单击", u8"右键单击", u8"中键单击",
    u8"按住", u8"释放按键",
    u8"滚轮上", u8"滚轮下",
    u8"5 切左键", u8"5 切右键",
    u8"启用/停用",
};

} // namespace

bool loadConfig(const wchar_t* iniPath, Config& out)
{
    out = defaultConfig();

    const DWORD attr = GetFileAttributesW(iniPath);
    const bool existed = (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
    if (!existed)
        return false;

    const MotionParams def = defaultMotionParams();
    const FloatParam params[] = {
        { L"vStart", &MotionParams::vStart, def.vStart, 1.f, 2000.f },
        { L"vMax", &MotionParams::vMax, def.vMax, 100.f, 8000.f },
        { L"rampMs", &MotionParams::rampMs, def.rampMs, 1.f, 5000.f },
        { L"gamma", &MotionParams::gamma, def.gamma, 0.1f, 4.f },
        { L"diagScale", &MotionParams::diagScale, def.diagScale, 0.5f, 1.f },
        { L"wheelRate", &MotionParams::wheelRate, def.wheelRate, 1.f, 60.f },
    };
    for (const auto& fp : params)
        readFloat(iniPath, L"motion", fp.key, fp.def, fp.lo, fp.hi, out.motion.*(fp.field));

    for (int i = 1; i < kActionCount; ++i)
        readScan(iniPath, L"keys", actionKey(i), out.bindings[i].scan, out.bindings[i].scan);

    if (out.bindings[(int)Action::Release].scan == 0x53 &&
        (out.bindings[(int)Action::RightClick].scan == 0 ||
         out.bindings[(int)Action::RightClick].scan == 0x53)) {
        out.bindings[(int)Action::RightClick].scan = 0x53;
        out.bindings[(int)Action::Release].scan = 0;
    }

    readBool(iniPath, L"misc", L"enabled", out.enabled, out.enabled);
    readBool(iniPath, L"misc", L"csvLog", out.csvLog, out.csvLog);
    readBool(iniPath, L"misc", L"fixNumLock", out.fixNumLock, out.fixNumLock);
    return true;
}

bool saveConfig(const wchar_t* iniPath, const Config& cfg)
{
    writeFloat(iniPath, L"motion", L"vStart", cfg.motion.vStart);
    writeFloat(iniPath, L"motion", L"vMax", cfg.motion.vMax);
    writeFloat(iniPath, L"motion", L"rampMs", cfg.motion.rampMs);
    writeFloat(iniPath, L"motion", L"gamma", cfg.motion.gamma);
    writeFloat(iniPath, L"motion", L"diagScale", cfg.motion.diagScale);
    writeFloat(iniPath, L"motion", L"wheelRate", cfg.motion.wheelRate);

    for (int i = 1; i < kActionCount; ++i)
        writeScan(iniPath, L"keys", actionKey(i), cfg.bindings[i].scan);

    writeBool(iniPath, L"misc", L"enabled", cfg.enabled);
    writeBool(iniPath, L"misc", L"csvLog", cfg.csvLog);
    writeBool(iniPath, L"misc", L"fixNumLock", cfg.fixNumLock);
    return true;
}

const char* actionName(Action a)
{
    int i = (int)a;
    if (i < 0 || i >= kActionCount)
        return "";
    return g_actionNames[i];
}

const char* scanName(unsigned short scan)
{
    if (const char* n = lookupScanName(scan))
        return n;
    static char fallback[8];
    snprintf(fallback, sizeof(fallback), "0x%X", (unsigned)scan);
    return fallback;
}
