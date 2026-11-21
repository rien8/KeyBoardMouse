#include "doctest.h"
#include "hook.h"
#include "config.h"

namespace {
KeyState routerWithDefaults()
{
    KeyState ks;
    Config c = defaultConfig();
    ks.setBindings(c.bindings, kActionCount);
    return ks;
}
}

TEST_CASE("unbound keys pass through untouched")
{
    KeyState ks = routerWithDefaults();
    KeyOutcome o = ks.onKey({ 0x02, false, false });
    CHECK_FALSE(o.swallow);
    CHECK(o.action == Action::None);
    CHECK(ks.directionMask() == 0u);
    o = ks.onKey({ 0x02, false, true });
    CHECK_FALSE(o.swallow);
}

TEST_CASE("bound key down is swallowed and sets direction bits")
{
    KeyState ks = routerWithDefaults();
    KeyOutcome o = ks.onKey({ 0x48, false, false });
    CHECK(o.swallow);
    CHECK(o.action == Action::Up);
    CHECK_FALSE(o.repeat);
    CHECK(ks.directionMask() == DIR_UP);
}

TEST_CASE("bound key up is swallowed and clears its bits")
{
    KeyState ks = routerWithDefaults();
    ks.onKey({ 0x48, false, false });
    KeyOutcome o = ks.onKey({ 0x48, false, true });
    CHECK(o.swallow);
    CHECK(o.action == Action::Up);
    CHECK(ks.directionMask() == 0u);
}

TEST_CASE("auto-repeat down events are swallowed but ignored")
{
    KeyState ks = routerWithDefaults();
    ks.onKey({ 0x48, false, false });
    KeyOutcome o = ks.onKey({ 0x48, false, false });
    CHECK(o.swallow);
    CHECK(o.repeat);
    CHECK(o.action == Action::None);
    CHECK(ks.directionMask() == DIR_UP);
}

TEST_CASE("up event for a key we never saw down keeps state consistent")
{
    KeyState ks = routerWithDefaults();
    KeyOutcome o = ks.onKey({ 0x4B, false, true });
    CHECK(o.swallow);
    CHECK(ks.directionMask() == 0u);
}

TEST_CASE("diagonal overlap: releasing one key keeps the shared direction")
{
    KeyState ks = routerWithDefaults();
    ks.onKey({ 0x47, false, false });
    CHECK(ks.directionMask() == (DIR_UP | DIR_LEFT));
    ks.onKey({ 0x48, false, false });
    CHECK(ks.directionMask() == (DIR_UP | DIR_LEFT));
    ks.onKey({ 0x47, false, true });
    CHECK(ks.directionMask() == DIR_UP);
    ks.onKey({ 0x48, false, true });
    CHECK(ks.directionMask() == 0u);
}

TEST_CASE("extended flag separates numpad enter from main enter")
{
    KeyState ks = routerWithDefaults();
    Config c = defaultConfig();
    c.bindings[(int)Action::Up].scan = static_cast<unsigned short>(0x1C | SCAN_EXT);
    ks.setBindings(c.bindings, kActionCount);

    KeyOutcome ext = ks.onKey({ 0x1C, true, false });
    CHECK(ext.swallow);
    CHECK(ext.action == Action::Up);
    CHECK(ks.directionMask() == DIR_UP);

    KeyOutcome plain = ks.onKey({ 0x1C, false, false });
    CHECK_FALSE(plain.swallow);
    CHECK(plain.action == Action::None);
}

TEST_CASE("wheel bindings toggle wheel bits")
{
    KeyState ks = routerWithDefaults();
    ks.onKey({ 0x4E, false, false });
    CHECK(ks.wheelMask() == WHEEL_UP);
    ks.onKey({ 0x4A, false, false });
    CHECK(ks.wheelMask() == (WHEEL_UP | WHEEL_DOWN));
    ks.onKey({ 0x4E, false, true });
    CHECK(ks.wheelMask() == WHEEL_DOWN);
    ks.onKey({ 0x4A, false, true });
    CHECK(ks.wheelMask() == 0u);
}

TEST_CASE("click and hold actions do not touch direction bits")
{
    KeyState ks = routerWithDefaults();
    KeyOutcome click = ks.onKey({ 0x4C, false, false });
    CHECK(click.swallow);
    CHECK(click.action == Action::LeftClick);
    KeyOutcome hold = ks.onKey({ 0x52, false, false });
    CHECK(hold.swallow);
    CHECK(hold.action == Action::HoldLeft);
    CHECK(ks.directionMask() == 0u);
    CHECK(ks.wheelMask() == 0u);
}

TEST_CASE("capture mode grabs the next key and reports its scan")
{
    KeyState ks = routerWithDefaults();
    ks.setCaptureMode(true);
    KeyOutcome o = ks.onKey({ 0x2A, false, false });
    CHECK(o.swallow);
    CHECK(o.captured);
    CHECK(o.capturedScan == 0x2A);
    CHECK(ks.captureMode() == false);
    CHECK(ks.directionMask() == 0u);
}

TEST_CASE("capture records the extended bit in the scan code")
{
    KeyState ks = routerWithDefaults();
    ks.setCaptureMode(true);
    KeyOutcome o = ks.onKey({ 0x1D, true, false });
    CHECK(o.captured);
    CHECK(o.capturedScan == (0x1D | SCAN_EXT));
}

TEST_CASE("esc cancels capture instead of binding")
{
    KeyState ks = routerWithDefaults();
    ks.setCaptureMode(true);
    KeyOutcome o = ks.onKey({ 0x01, false, false });
    CHECK(o.swallow);
    CHECK(o.cancelled);
    CHECK(o.capturedScan == 0);
    CHECK(ks.captureMode() == false);
}

TEST_CASE("key up events are ignored by capture mode")
{
    KeyState ks = routerWithDefaults();
    ks.setCaptureMode(true);
    KeyOutcome up = ks.onKey({ 0x48, false, true });
    CHECK(up.swallow);
    CHECK_FALSE(up.captured);
    CHECK(up.capturedScan == 0);
    CHECK(ks.captureMode() == true);
    KeyOutcome down = ks.onKey({ 0x1F, false, false });
    CHECK(down.captured);
    CHECK(down.capturedScan == 0x1F);
}

TEST_CASE("empty binding table passes everything")
{
    KeyState ks;
    Binding empty[kActionCount] = {};
    ks.setBindings(empty, kActionCount);
    KeyOutcome o = ks.onKey({ 0x48, false, false });
    CHECK_FALSE(o.swallow);
    CHECK(o.action == Action::None);
}

TEST_CASE("duplicate scan on two actions routes to the first")
{
    Config c = defaultConfig();
    c.bindings[(int)Action::Down].scan = 0x48;
    KeyState ks;
    ks.setBindings(c.bindings, kActionCount);
    KeyOutcome o = ks.onKey({ 0x48, false, false });
    CHECK(o.swallow);
    CHECK(o.action == Action::Up);
}
