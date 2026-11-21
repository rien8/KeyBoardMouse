#pragma once

#include "config.h"

#include <windows.h>
#include <atomic>

double speedAt(const MotionParams& p, double holdMs);

struct TickOutput {
    int dx;
    int dy;
    int wheel;
};

class MotionCore {
public:
    explicit MotionCore(const MotionParams& p);

    void setParams(const MotionParams& p);
    void setDirection(unsigned mask);
    void setWheel(int dir);

    TickOutput tick(double dt);
    double speed() const;
    double holdMs() const;
    void resetHold();

private:
    MotionParams m_p;
    unsigned m_dir;
    int m_wheel;
    double m_holdMs;
    double m_accX;
    double m_accY;
    double m_accWheel;
    double m_speed;
};

class MotionEngine {
public:
    MotionEngine();
    ~MotionEngine();
    MotionEngine(const MotionEngine&) = delete;
    MotionEngine& operator=(const MotionEngine&) = delete;

    void start(const MotionParams& params, bool csvLog);
    void stop();

    void setParams(const MotionParams& params);
    void setInput(unsigned mask);
    void status(unsigned& mask, double& speedPxS) const;

private:
    static DWORD WINAPI threadProc(LPVOID self);
    void run();

    HANDLE m_thread;
    HANDLE m_wakeEvent;
    std::atomic<bool> m_running;
    std::atomic<unsigned> m_input;
    mutable SRWLOCK m_lock;
    MotionParams m_params;
    bool m_csvLog;
    unsigned m_statusMask;
    double m_statusSpeed;
    MotionCore m_core;
};
