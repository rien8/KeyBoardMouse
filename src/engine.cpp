#include "engine.h"

#include "inject.h"

#include <cmath>
#include <cstdio>

double speedAt(const MotionParams& p, double holdMs)
{
    if (p.rampMs <= 0.f)
        return p.vMax;
    double t = holdMs / (double)p.rampMs;
    if (t >= 1.0)
        return p.vMax;
    if (t <= 0.0)
        return p.vStart;
    const double shape = std::pow(t, (double)p.gamma);
    return (double)p.vStart + ((double)p.vMax - (double)p.vStart) * shape;
}

MotionCore::MotionCore(const MotionParams& p)
    : m_p(p)
    , m_dir(0)
    , m_wheel(0)
    , m_holdMs(0.0)
    , m_accX(0.0)
    , m_accY(0.0)
    , m_accWheel(0.0)
    , m_speed(0.0)
{
}

void MotionCore::setParams(const MotionParams& p)
{
    m_p = p;
}

void MotionCore::setDirection(unsigned mask)
{
    m_dir = mask;
}

void MotionCore::setWheel(int dir)
{
    m_wheel = dir;
}

void MotionCore::resetHold()
{
    m_holdMs = 0.0;
    m_speed = 0.0;
}

double MotionCore::speed() const
{
    return m_speed;
}

double MotionCore::holdMs() const
{
    return m_holdMs;
}

TickOutput MotionCore::tick(double dt)
{
    TickOutput out = { 0, 0, 0 };
    if (dt <= 0.0)
        return out;

    if (m_dir != 0) {
        m_holdMs += dt * 1000.0;
        m_speed = speedAt(m_p, m_holdMs);

        const int ux = ((m_dir & DIR_RIGHT) ? 1 : 0) - ((m_dir & DIR_LEFT) ? 1 : 0);
        const int uy = ((m_dir & DIR_DOWN) ? 1 : 0) - ((m_dir & DIR_UP) ? 1 : 0);
        const double scale = (ux != 0 && uy != 0) ? (double)m_p.diagScale : 1.0;

        m_accX += (double)ux * scale * m_speed * dt;
        m_accY += (double)uy * scale * m_speed * dt;
    } else {
        m_holdMs = 0.0;
        m_speed = 0.0;
    }

    out.dx = (int)m_accX;
    m_accX -= (double)out.dx;
    out.dy = (int)m_accY;
    m_accY -= (double)out.dy;

    if (m_wheel != 0) {
        m_accWheel += (double)m_wheel * (double)m_p.wheelRate * dt;
        out.wheel = (int)m_accWheel;
        m_accWheel -= (double)out.wheel;
    }

    return out;
}

MotionEngine::MotionEngine()
    : m_thread(nullptr)
    , m_wakeEvent(nullptr)
    , m_running(false)
    , m_input(0)
    , m_lock(SRWLOCK_INIT)
    , m_params(defaultMotionParams())
    , m_csvLog(false)
    , m_statusMask(0)
    , m_statusSpeed(0.0)
    , m_core(defaultMotionParams())
{
}

MotionEngine::~MotionEngine()
{
    stop();
}

void MotionEngine::start(const MotionParams& params, bool csvLog)
{
    stop();
    AcquireSRWLockExclusive(&m_lock);
    m_params = params;
    m_csvLog = csvLog;
    ReleaseSRWLockExclusive(&m_lock);
    m_core = MotionCore(params);
    m_running.store(true, std::memory_order_relaxed);
    m_input.store(0, std::memory_order_relaxed);
    if (!m_wakeEvent)
        m_wakeEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    m_thread = CreateThread(nullptr, 0, threadProc, this, 0, nullptr);
}

void MotionEngine::stop()
{
    if (!m_thread)
        return;
    m_running.store(false, std::memory_order_relaxed);
    if (m_wakeEvent)
        SetEvent(m_wakeEvent);
    WaitForSingleObject(m_thread, 5000);
    CloseHandle(m_thread);
    m_thread = nullptr;
    if (m_wakeEvent) {
        CloseHandle(m_wakeEvent);
        m_wakeEvent = nullptr;
    }
}

void MotionEngine::setParams(const MotionParams& params)
{
    AcquireSRWLockExclusive(&m_lock);
    m_params = params;
    ReleaseSRWLockExclusive(&m_lock);
}

void MotionEngine::setInput(unsigned mask)
{
    m_input.store(mask, std::memory_order_relaxed);
    if (m_wakeEvent)
        SetEvent(m_wakeEvent);
}

void MotionEngine::status(unsigned& mask, double& speedPxS) const
{
    AcquireSRWLockShared(&m_lock);
    mask = m_statusMask;
    speedPxS = m_statusSpeed;
    ReleaseSRWLockShared(&m_lock);
}

DWORD WINAPI MotionEngine::threadProc(LPVOID self)
{
    static_cast<MotionEngine*>(self)->run();
    return 0;
}

namespace {

FILE* openTickCsv()
{
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    wchar_t* slash = wcsrchr(path, L'\\');
    if (slash)
        slash[1] = L'\0';
    wcscat_s(path, L"mousekey_ticks.csv");
    FILE* f = nullptr;
    if (_wfopen_s(&f, path, L"w") == 0 && f) {
        setvbuf(f, nullptr, _IOFBF, 64 * 1024);
        fprintf(f, "t_ms,dt_ms,dir,v_px_s,dx,dy,wheel\n");
        return f;
    }
    return nullptr;
}

} // namespace

void MotionEngine::run()
{
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

    LARGE_INTEGER freq = {};
    QueryPerformanceFrequency(&freq);
    const double toMs = 1000.0 / (double)freq.QuadPart;

    LARGE_INTEGER prev = {};
    QueryPerformanceCounter(&prev);
    double clockMs = 0.0;

    AcquireSRWLockShared(&m_lock);
    const bool wantCsv = m_csvLog;
    ReleaseSRWLockShared(&m_lock);
    FILE* csv = wantCsv ? openTickCsv() : nullptr;

    bool active = false;
    bool timerRaised = false;
    bool wasMoving = false;

    while (m_running.load(std::memory_order_relaxed)) {
        const unsigned input = m_input.load(std::memory_order_relaxed);

        if (input == 0) {
            if (active) {
                active = false;
                m_core.setDirection(0);
                m_core.setWheel(0);
                m_core.resetHold();
                if (timerRaised) {
                    timeEndPeriod(1);
                    timerRaised = false;
                }
                AcquireSRWLockExclusive(&m_lock);
                m_statusMask = 0;
                m_statusSpeed = 0.0;
                ReleaseSRWLockExclusive(&m_lock);
                if (csv && wasMoving)
                    fprintf(csv, "%.3f,0.000,0x0,0.00,0,0,0\n", clockMs);
                wasMoving = false;
            }
            WaitForSingleObject(m_wakeEvent, INFINITE);
            QueryPerformanceCounter(&prev);
            if (!m_running.load(std::memory_order_relaxed))
                break;
            continue;
        }

        if (!active) {
            active = true;
            timeBeginPeriod(1);
            timerRaised = true;
            QueryPerformanceCounter(&prev);
        }

        LARGE_INTEGER now = {};
        QueryPerformanceCounter(&now);
        double dtMs = (double)(now.QuadPart - prev.QuadPart) * toMs;
        prev = now;
        if (dtMs <= 0.0)
            dtMs = 4.0;
        if (dtMs > 100.0)
            dtMs = 100.0;
        clockMs += dtMs;

        AcquireSRWLockShared(&m_lock);
        const MotionParams p = m_params;
        ReleaseSRWLockShared(&m_lock);

        m_core.setParams(p);
        m_core.setDirection(input & (DIR_UP | DIR_DOWN | DIR_LEFT | DIR_RIGHT));
        const int wheelDir =
            ((input & WHEEL_UP) ? 1 : 0) - ((input & WHEEL_DOWN) ? 1 : 0);
        m_core.setWheel(wheelDir);
        const TickOutput o = m_core.tick(dtMs / 1000.0);

        if (o.dx || o.dy)
            inject::move(o.dx, o.dy);
        if (o.wheel)
            inject::wheel(o.wheel);

        AcquireSRWLockExclusive(&m_lock);
        m_statusMask = input;
        m_statusSpeed = m_core.speed();
        ReleaseSRWLockExclusive(&m_lock);

        if (csv) {
            fprintf(csv, "%.3f,%.3f,0x%X,%.2f,%d,%d,%d\n",
                    clockMs, dtMs, input & 0xF, m_core.speed(), o.dx, o.dy, o.wheel);
            wasMoving = true;
        }

        Sleep(4);
    }

    if (csv)
        fclose(csv);
    if (timerRaised)
        timeEndPeriod(1);
}
