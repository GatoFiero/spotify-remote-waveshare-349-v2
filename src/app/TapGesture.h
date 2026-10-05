#pragma once
#include <stdint.h>
namespace ui {
enum class TapPhase : uint8_t { Idle, Start, Hold, Release };
class TapGesture {
    bool held = false, valid = false;
    int16_t start_x = 0, start_y = 0;
public:
    TapPhase update(bool down, int16_t x, int16_t y) {
        if (!down) {
            if (!held) return TapPhase::Idle;
            held = false;
            return TapPhase::Release;
        }
        if (!held) {
            held = true; valid = true; start_x = x; start_y = y;
            return TapPhase::Start;
        }
        const int dx = x-start_x, dy = y-start_y;
        if (dx > 24 || dx < -24 || dy > 24 || dy < -24) valid = false;
        return TapPhase::Hold;
    }
    bool accepted() const { return valid; }
};
}
