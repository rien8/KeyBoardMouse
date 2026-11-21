#include "doctest.h"
#include "config.h"

#include <windows.h>
#include <cstdio>
#include <cstring>
#include <string>

namespace {
std::wstring tempIni(const char* name)
{
    wchar_t dir[MAX_PATH];
    GetTempPathW(MAX_PATH, dir);
    std::wstring p(dir);
    p += L"mk_test_";
    for (const char* c = name; *c; ++c)
        p += static_cast<wchar_t>(static_cast<unsigned char>(*c));
    p += L".ini";
    DeleteFileW(p.c_str());
    return p;
}

void writeRawIni(const std::wstring& path, const char* text)
{
    FILE* f = nullptr;
    _wfopen_s(&f, path.c_str(), L"wb");
    REQUIRE(f != nullptr);
    fputs(text, f);
    fclose(f);
}
}

TEST_CASE("default motion params match the plan")
{
    MotionParams p = defaultMotionParams();
    CHECK(p.vStart == doctest::Approx(40.f));
    CHECK(p.vMax == doctest::Approx(1600.f));
    CHECK(p.rampMs == doctest::Approx(500.f));
    CHECK(p.gamma == doctest::Approx(1.0f));
    CHECK(p.diagScale == doctest::Approx(0.7071f).epsilon(0.002));
    CHECK(p.wheelRate == doctest::Approx(12.f));
}

TEST_CASE("default bindings follow the windows mouse-keys numpad")
{
    Config c = defaultConfig();
    CHECK(c.bindings[(int)Action::Up].scan == 0x48);
    CHECK(c.bindings[(int)Action::Down].scan == 0x50);
    CHECK(c.bindings[(int)Action::Left].scan == 0x4B);
    CHECK(c.bindings[(int)Action::Right].scan == 0x4D);
    CHECK(c.bindings[(int)Action::UpLeft].scan == 0x47);
    CHECK(c.bindings[(int)Action::UpRight].scan == 0x49);
    CHECK(c.bindings[(int)Action::DownLeft].scan == 0x4F);
    CHECK(c.bindings[(int)Action::DownRight].scan == 0x51);
    CHECK(c.bindings[(int)Action::Click].scan == 0x4C);
    CHECK(c.bindings[(int)Action::RightClick].scan == 0x53);
    CHECK(c.bindings[(int)Action::Hold].scan == 0x52);
    CHECK(c.bindings[(int)Action::Release].scan == 0);
    CHECK(c.bindings[(int)Action::WheelUp].scan == 0x4E);
    CHECK(c.bindings[(int)Action::WheelDown].scan == 0x4A);
    CHECK(c.bindings[(int)Action::ModeLeft].scan == 0x135);
    CHECK(c.bindings[(int)Action::ModeRight].scan == 0x37);
    CHECK(c.bindings[(int)Action::ToggleEnabled].scan == 0x45);
    CHECK(c.bindings[(int)Action::MiddleClick].scan == 0);
    CHECK(c.bindings[(int)Action::None].scan == 0);

    for (int i = 0; i < kActionCount; ++i)
        CHECK(c.bindings[i].action == static_cast<Action>(i));
}

TEST_CASE("loading a missing file yields defaults")
{
    auto p = tempIni("missing");
    DeleteFileW(p.c_str());
    Config c;
    c.motion.vMax = 0.f;
    c.bindings[(int)Action::Up].scan = 0;
    bool existed = loadConfig(p.c_str(), c);
    CHECK_FALSE(existed);
    CHECK(c.motion.vMax == doctest::Approx(1600.f));
    CHECK(c.bindings[(int)Action::Up].scan == 0x48);
}

TEST_CASE("save then load round-trips modified values")
{
    auto p = tempIni("roundtrip");
    Config c = defaultConfig();
    c.motion.vStart = 66.f;
    c.motion.vMax = 2400.f;
    c.motion.rampMs = 300.f;
    c.motion.gamma = 1.4f;
    c.motion.diagScale = 0.8f;
    c.motion.wheelRate = 20.f;
    c.bindings[(int)Action::Up].scan = 0x11;
    c.bindings[(int)Action::WheelDown].scan = 0x1F;
    c.bindings[(int)Action::RightClick].scan = 0x115;
    c.bindings[(int)Action::Click].scan = 0;
    c.enabled = false;
    c.csvLog = true;

    REQUIRE(saveConfig(p.c_str(), c));

    Config d;
    bool existed = loadConfig(p.c_str(), d);
    REQUIRE(existed);
    CHECK(d.motion.vStart == doctest::Approx(66.f));
    CHECK(d.motion.vMax == doctest::Approx(2400.f));
    CHECK(d.motion.rampMs == doctest::Approx(300.f));
    CHECK(d.motion.gamma == doctest::Approx(1.4f));
    CHECK(d.motion.diagScale == doctest::Approx(0.8f));
    CHECK(d.motion.wheelRate == doctest::Approx(20.f));
    CHECK(d.bindings[(int)Action::Up].scan == 0x11);
    CHECK(d.bindings[(int)Action::WheelDown].scan == 0x1F);
    CHECK(d.bindings[(int)Action::RightClick].scan == 0x115);
    CHECK(d.bindings[(int)Action::Click].scan == 0);
    CHECK(d.bindings[(int)Action::Down].scan == 0x50);
    CHECK_FALSE(d.enabled);
    CHECK(d.csvLog);
}

TEST_CASE("hand-written ini parses hex and decimal scans, partial sections")
{
    auto p = tempIni("handwrite");
    writeRawIni(p,
        "[motion]\r\n"
        "vStart=15\r\n"
        "vMax=900\r\n"
        "rampMs=420\r\n"
        "gamma=0.8\r\n"
        "diagScale=0.71\r\n"
        "wheelRate=5\r\n"
        "[keys]\r\n"
        "Up=0x48\r\n"
        "Down=80\r\n"
        "[misc]\r\n"
        "enabled=0\r\n"
        "csvLog=1\r\n");

    Config c;
    REQUIRE(loadConfig(p.c_str(), c));
    CHECK(c.motion.vStart == doctest::Approx(15.f));
    CHECK(c.motion.vMax == doctest::Approx(900.f));
    CHECK(c.motion.rampMs == doctest::Approx(420.f));
    CHECK(c.motion.gamma == doctest::Approx(0.8f));
    CHECK(c.motion.diagScale == doctest::Approx(0.71f));
    CHECK(c.motion.wheelRate == doctest::Approx(5.f));
    CHECK(c.bindings[(int)Action::Up].scan == 0x48);
    CHECK(c.bindings[(int)Action::Down].scan == 80);
    CHECK(c.bindings[(int)Action::Left].scan == 0x4B);
    CHECK_FALSE(c.enabled);
    CHECK(c.csvLog);
}

TEST_CASE("out-of-range values fall back to defaults")
{
    auto p = tempIni("bounds");
    writeRawIni(p,
        "[motion]\r\n"
        "vStart=-5\r\n"
        "vMax=abc\r\n"
        "rampMs=0\r\n"
        "gamma=0\r\n"
        "diagScale=9\r\n"
        "wheelRate=-1\r\n"
        "[keys]\r\n"
        "Up=99999\r\n");

    Config c;
    REQUIRE(loadConfig(p.c_str(), c));
    CHECK(c.motion.vStart == doctest::Approx(40.f));
    CHECK(c.motion.vMax == doctest::Approx(1600.f));
    CHECK(c.motion.rampMs == doctest::Approx(500.f));
    CHECK(c.motion.gamma == doctest::Approx(1.0f));
    CHECK(c.motion.diagScale == doctest::Approx(0.7071f).epsilon(0.002));
    CHECK(c.motion.wheelRate == doctest::Approx(12.f));
    CHECK(c.bindings[(int)Action::Up].scan == 0x48);
}

TEST_CASE("action names cover every action with distinct non-empty labels")
{
    const char* names[kActionCount] = {};
    for (int i = 1; i < kActionCount; ++i) {
        const char* n = actionName(static_cast<Action>(i));
        CHECK(n != nullptr);
        CHECK(*n != '\0');
        names[i] = n;
    }
    CHECK(std::string(actionName(Action::Up)) == u8"上");
    CHECK(std::string(actionName(Action::Click)) == u8"单击");
    CHECK(std::string(actionName(Action::Hold)) == u8"按住");
    CHECK(std::string(actionName(Action::WheelUp)) == u8"滚轮上");
    for (int i = 1; i < kActionCount; ++i)
        for (int j = i + 1; j < kActionCount; ++j)
            CHECK(std::strcmp(names[i], names[j]) != 0);
}

TEST_CASE("numlock fix is on by default and round-trips")
{
    Config def = defaultConfig();
    CHECK(def.fixNumLock);

    auto p = tempIni("numlock");
    Config c = defaultConfig();
    c.fixNumLock = false;
    REQUIRE(saveConfig(p.c_str(), c));

    Config d;
    REQUIRE(loadConfig(p.c_str(), d));
    CHECK_FALSE(d.fixNumLock);
}

TEST_CASE("hand-written ini can turn the numlock fix off")
{
    auto p = tempIni("numlockraw");
    writeRawIni(p,
        "[misc]\r\n"
        "fixNumLock=0\r\n");

    Config c;
    REQUIRE(loadConfig(p.c_str(), c));
    CHECK_FALSE(c.fixNumLock);
}

TEST_CASE("old ini without the numlock key keeps the default")
{
    auto p = tempIni("numlockold");
    writeRawIni(p,
        "[misc]\r\n"
        "csvLog=1\r\n");

    Config c;
    REQUIRE(loadConfig(p.c_str(), c));
    CHECK(c.fixNumLock);
}

TEST_CASE("scan names distinguish numpad, extended and unknown keys")
{
    CHECK(std::string(scanName(0x48)) == u8"小键盘 8");
    CHECK(std::string(scanName(0x4E)) == u8"小键盘 +");
    CHECK(std::string(scanName(0x53)) == u8"小键盘 .");
    CHECK(std::string(scanName(0x1C)) == "Enter");
    CHECK(std::string(scanName(0x11C)) == u8"小键盘 Enter");
    CHECK(std::string(scanName(0x11D)) == "Ctrl");
    CHECK(std::string(scanName(0x1D)) == "Ctrl");
    CHECK(std::string(scanName(0x1F)) == "S");
    CHECK(std::string(scanName(0x11)) == "W");
    CHECK(std::string(scanName(0x5B)) == u8"左 Win");
    CHECK(std::string(scanName(0x5B | SCAN_EXT)) == u8"左 Win");
    CHECK(std::string(scanName(0x48 | SCAN_EXT)) != u8"小键盘 8");
    CHECK(std::string(scanName(0x145)) == "NumLock");
    CHECK(std::string(scanName(0x45)) == "NumLock");
    std::string unknown = scanName(0x180);
    CHECK(unknown.rfind("0x", 0) == 0);
}

TEST_CASE("old ini with Release on numpad dot migrates to right click")
{
    auto p = tempIni("migrate");
    writeRawIni(p,
        "[keys]\r\n"
        "Release=0x53\r\n");

    Config c;
    REQUIRE(loadConfig(p.c_str(), c));
    CHECK(c.bindings[(int)Action::RightClick].scan == 0x53);
    CHECK(c.bindings[(int)Action::Release].scan == 0);
}

TEST_CASE("explicit right click binding is not overwritten by migration")
{
    auto p = tempIni("nomigrate");
    writeRawIni(p,
        "[keys]\r\n"
        "Release=0x53\r\n"
        "RightClick=0x4A\r\n");

    Config c;
    REQUIRE(loadConfig(p.c_str(), c));
    CHECK(c.bindings[(int)Action::RightClick].scan == 0x4A);
    CHECK(c.bindings[(int)Action::Release].scan == 0x53);
}
