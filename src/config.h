#pragma once

enum {
    DIR_UP = 1,
    DIR_DOWN = 2,
    DIR_LEFT = 4,
    DIR_RIGHT = 8,
    WHEEL_UP = 16,
    WHEEL_DOWN = 32,
    SCAN_EXT = 0x100,
};

struct MotionParams {
    float vStart;
    float vMax;
    float rampMs;
    float gamma;
    float diagScale;
    float wheelRate;
};

enum class Action : unsigned char {
    None = 0,
    Up,
    Down,
    Left,
    Right,
    UpLeft,
    UpRight,
    DownLeft,
    DownRight,
    Click,
    RightClick,
    MiddleClick,
    Hold,
    Release,
    WheelUp,
    WheelDown,
    ModeLeft,
    ModeRight,
    ToggleEnabled,
    Count
};

constexpr int kActionCount = static_cast<int>(Action::Count);

struct Binding {
    unsigned short scan;
    Action action;
};

struct Config {
    MotionParams motion;
    Binding bindings[kActionCount];
    bool enabled;
    bool csvLog;
    bool fixNumLock;
};

MotionParams defaultMotionParams();
Config defaultConfig();
bool loadConfig(const wchar_t* iniPath, Config& out);
bool saveConfig(const wchar_t* iniPath, const Config& cfg);
const char* actionName(Action a);
const char* scanName(unsigned short scan);
