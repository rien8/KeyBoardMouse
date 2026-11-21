#pragma once

#include "config.h"

class App;

namespace ui {

struct UiState {
    int captureAction = 0;
    int conflictAction = 0;
    unsigned short conflictScan = 0;
    float savedFlash = 0.0f;
};

void draw(App& app, UiState& st);

}
