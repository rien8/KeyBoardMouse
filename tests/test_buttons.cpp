#include "doctest.h"
#include "buttons.h"

TEST_CASE("press marks a button held, second press is ignored")
{
    ButtonState b;
    CHECK(b.mask() == 0);
    CHECK(b.press(0));
    CHECK(b.isDown(0));
    CHECK_FALSE(b.press(0));
    CHECK(b.mask() == 1);
}

TEST_CASE("release only succeeds for a button that is actually held")
{
    ButtonState b;
    CHECK_FALSE(b.release(1));
    CHECK(b.mask() == 0);

    b.press(1);
    CHECK(b.release(1));
    CHECK_FALSE(b.isDown(1));
    CHECK_FALSE(b.release(1));
}

TEST_CASE("takeAll returns held buttons once and clears the state")
{
    ButtonState b;
    b.press(0);
    b.press(2);
    CHECK(b.takeAll() == 5);
    CHECK(b.mask() == 0);
    CHECK(b.takeAll() == 0);
}

TEST_CASE("no buttons held means nothing to release")
{
    ButtonState b;
    CHECK(b.takeAll() == 0);
}

TEST_CASE("out of range buttons are rejected")
{
    ButtonState b;
    CHECK_FALSE(b.press(-1));
    CHECK_FALSE(b.press(ButtonState::kMaxButtons));
    CHECK_FALSE(b.release(-1));
    CHECK(b.mask() == 0);
}

TEST_CASE("independent buttons keep separate state")
{
    ButtonState b;
    b.press(0);
    b.press(1);
    CHECK(b.release(0));
    CHECK(b.isDown(1));
    CHECK_FALSE(b.isDown(0));
    CHECK(b.takeAll() == 2);
}
