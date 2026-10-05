#pragma once
#include <cstdint>
namespace ui {
enum class Button : uint8_t { None, Previous, PlayPause, Next, Like };
inline Button playerButtonAt(int16_t x, int16_t y) {
    // Preserve the original generous transport targets. The heart occupies
    // a separate bottom row and cannot steal a low transport tap.
    if (y >= 24 && y <= 128) {
        if (x >= 486 && x <= 537) return Button::Previous;
        if (x >= 538 && x <= 585) return Button::PlayPause;
        if (x >= 586 && x <= 634) return Button::Next;
    }
    if (x >= 490 && x <= 542 && y >= 130 && y <= 171) return Button::Like;
    return Button::None;
}
} // namespace ui
