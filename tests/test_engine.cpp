#include "doctest.h"
#include "engine.h"
#include "config.h"

#include <algorithm>
#include <cmath>

using doctest::Approx;

namespace {
const double DT = 0.004;

MotionParams flatParams(double v)
{
    MotionParams p;
    p.vStart = static_cast<float>(v);
    p.vMax = static_cast<float>(v);
    p.rampMs = 1.f;
    p.gamma = 1.f;
    p.diagScale = 0.7071f;
    p.wheelRate = 12.f;
    return p;
}
}

TEST_CASE("curve endpoints: v(0)=vStart, v(rampMs)=vMax, clamped after")
{
    MotionParams p = defaultMotionParams();
    CHECK(speedAt(p, 0.0) == Approx(p.vStart));
    CHECK(speedAt(p, p.rampMs) == Approx(p.vMax));
    CHECK(speedAt(p, 10.0 * p.rampMs) == Approx(p.vMax));
}

TEST_CASE("curve is monotonic non-decreasing for several gammas")
{
    const float gammas[] = { 0.4f, 1.0f, 2.5f };
    for (float g : gammas) {
        MotionParams p = defaultMotionParams();
        p.gamma = g;
        double last = -1.0;
        for (double t = 0.0; t <= p.rampMs + 1.0; t += 5.0) {
            double v = speedAt(p, t);
            CHECK(v >= last);
            last = v;
        }
        CHECK(speedAt(p, 0.0) == Approx(p.vStart).epsilon(0.001));
        CHECK(speedAt(p, p.rampMs) == Approx(p.vMax).epsilon(0.001));
    }
}

TEST_CASE("gamma bends the ramp around the linear midpoint")
{
    MotionParams lo = defaultMotionParams();
    lo.gamma = 0.5f;
    MotionParams hi = defaultMotionParams();
    hi.gamma = 2.0f;
    MotionParams lin = defaultMotionParams();
    lin.gamma = 1.0f;
    double mid = lin.rampMs / 2.0;
    double halfV = (lin.vStart + lin.vMax) / 2.0;
    CHECK(speedAt(lo, mid) > halfV);
    CHECK(speedAt(hi, mid) < halfV);
    CHECK(speedAt(lin, mid) == Approx(halfV).epsilon(0.001));
}

TEST_CASE("rampMs<=0 means instant full speed")
{
    MotionParams p = flatParams(100.0);
    p.rampMs = 0.f;
    CHECK(speedAt(p, 0.0) == Approx(p.vMax));
}

TEST_CASE("acceptance: ramp reaches vMax within one tick of rampMs")
{
    MotionParams p = defaultMotionParams();
    MotionCore c(p);
    c.setDirection(DIR_UP);
    int reachedAt = -1;
    for (int i = 0; i < 400; ++i) {
        c.tick(DT);
        if (reachedAt < 0 && c.speed() >= p.vMax - 0.5)
            reachedAt = i;
    }
    REQUIRE(reachedAt > 0);
    double reachedMs = (reachedAt + 1) * 4.0;
    CHECK(reachedMs <= p.rampMs + 4.0);
    CHECK(reachedMs >= p.rampMs - 4.0);
}

TEST_CASE("acceptance: constant 100 px/s for 1s moves ~100 px")
{
    MotionCore c(flatParams(100.0));
    c.setDirection(DIR_DOWN);
    long long dy = 0;
    for (int i = 0; i < 250; ++i)
        dy += c.tick(DT).dy;
    CHECK(dy >= 99);
    CHECK(dy <= 100);
}

TEST_CASE("acceptance: 10 px/s for 1s moves 10px +-1 with no stall window")
{
    MotionCore c(flatParams(10.0));
    c.setDirection(DIR_DOWN);
    long long total = 0;
    long long lastEmit = -1;
    long long maxGap = 0;
    for (long long i = 0; i < 250; ++i) {
        TickOutput o = c.tick(DT);
        total += o.dy;
        if (o.dy != 0) {
            if (lastEmit >= 0)
                maxGap = (std::max)(maxGap, i - lastEmit);
            lastEmit = i;
        }
    }
    CHECK(total >= 9);
    CHECK(total <= 11);
    CHECK(maxGap <= 26);
}

TEST_CASE("leftward motion does not emit premature pixels")
{
    MotionCore c(flatParams(10.0));
    c.setDirection(DIR_LEFT);
    TickOutput o = c.tick(DT);
    CHECK(o.dx == 0);
    long long dx = o.dx;
    for (int i = 1; i < 250; ++i)
        dx += c.tick(DT).dx;
    CHECK(dx >= -11);
    CHECK(dx <= -9);
}

TEST_CASE("acceptance: 45deg diagonal |dx-dy|<=1 and isotropic")
{
    MotionParams p = flatParams(100.0);
    MotionCore c(p);
    c.setDirection(DIR_UP | DIR_LEFT);
    long long dx = 0, dy = 0;
    for (int i = 0; i < 250; ++i) {
        TickOutput o = c.tick(DT);
        dx += o.dx;
        dy += o.dy;
    }
    CHECK(std::abs(dx - dy) <= 1);
    const double mag = std::sqrt((double)dx * dx + (double)dy * dy);
    CHECK(mag == Approx(100.0).epsilon(0.02));
}

TEST_CASE("diagScale=1 makes diagonal components equal straight speed")
{
    MotionParams p = flatParams(100.0);
    p.diagScale = 1.0f;
    MotionCore c(p);
    c.setDirection(DIR_DOWN | DIR_RIGHT);
    long long dx = 0, dy = 0;
    for (int i = 0; i < 250; ++i) {
        TickOutput o = c.tick(DT);
        dx += o.dx;
        dy += o.dy;
    }
    CHECK(dx >= 99);
    CHECK(dx <= 100);
    CHECK(dy >= 99);
    CHECK(dy <= 100);
}

TEST_CASE("wheel accumulates whole detents at wheelRate")
{
    MotionParams p = flatParams(100.0);
    p.wheelRate = 12.f;
    MotionCore c(p);
    c.setWheel(1);
    long long detents = 0;
    for (int i = 0; i < 250; ++i)
        detents += c.tick(DT).wheel;
    CHECK(detents >= 11);
    CHECK(detents <= 12);
}

TEST_CASE("wheel direction can flip and cancel")
{
    MotionParams p = flatParams(100.0);
    p.wheelRate = 100.f;
    MotionCore c(p);
    c.setWheel(1);
    c.tick(DT);
    c.setWheel(-1);
    c.tick(DT);
    c.setWheel(0);
    TickOutput o = c.tick(DT);
    CHECK(o.wheel == 0);
}

TEST_CASE("release resets the ramp only when all motion keys are up")
{
    MotionParams p = defaultMotionParams();
    MotionCore c(p);
    c.setDirection(DIR_RIGHT);
    for (int i = 0; i < 75; ++i)
        c.tick(DT);
    CHECK(c.holdMs() == Approx(300.0).epsilon(0.01));

    c.setDirection(DIR_RIGHT | DIR_UP);
    c.tick(DT);
    CHECK(c.holdMs() == Approx(304.0).epsilon(0.01));

    c.setDirection(DIR_UP);
    c.tick(DT);
    CHECK(c.holdMs() == Approx(308.0).epsilon(0.01));

    c.setDirection(0);
    c.tick(DT);
    CHECK(c.holdMs() == 0.0);
    CHECK(c.speed() == 0.0);

    c.setDirection(DIR_UP);
    c.tick(DT);
    CHECK(c.speed() == Approx(speedAt(p, 4.0)).epsilon(0.001));
}

TEST_CASE("zero dt is a no-op")
{
    MotionCore c(flatParams(100.0));
    c.setDirection(DIR_UP | DIR_LEFT);
    c.setWheel(1);
    TickOutput o = c.tick(0.0);
    CHECK(o.dx == 0);
    CHECK(o.dy == 0);
    CHECK(o.wheel == 0);
    CHECK(c.holdMs() == 0.0);
}

TEST_CASE("params can be swapped mid-flight keeping accumulators sane")
{
    MotionCore c(flatParams(100.0));
    c.setDirection(DIR_UP);
    for (int i = 0; i < 100; ++i)
        c.tick(DT);
    MotionParams p2 = flatParams(500.0);
    c.setParams(p2);
    TickOutput o = c.tick(DT);
    CHECK(c.speed() == Approx(500.0));
    CHECK(c.holdMs() > 0.0);
}
