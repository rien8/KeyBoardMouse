#pragma once

enum MouseButton {
    MKBTN_LEFT = 0,
    MKBTN_RIGHT = 1,
    MKBTN_MIDDLE = 2,
};

namespace inject {
void move(int dx, int dy);
void wheel(int detents);
void buttonDown(int button);
void buttonUp(int button);
void click(int button);
void releaseAllButtons();
}
