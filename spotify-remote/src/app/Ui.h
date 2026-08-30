#pragma once

#include "../platform/Hardware.h"
#include "NowPlaying.h"

namespace ui {

enum class Button : uint8_t { None, Previous, PlayPause, Next };

void begin();

// Which button, if any, is under a logical screen coordinate.
Button hitTest(int16_t x, int16_t y);

// Repaints whatever changed. Cheap to call every loop: it diffs against the last
// rendered state and touches nothing if nothing moved.
void render(const app::NowPlaying &now, const hardware::Battery &battery, Button pressed);

}  // namespace ui
