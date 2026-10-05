#pragma once
#include <stdint.h>
namespace ui {
class SwipeDown {
    bool active = false;
    bool eligible = false;
    int16_t start_x = 0, start_y = 0;
public:
    bool startedOnLeft() const { return start_x < 320; }
    bool update(bool down, int16_t x, int16_t y) {
        if (!down) { active = false; eligible = false; return false; }
        if (!active) {
            active = true;
            eligible = y <= 30;
            start_x = x; start_y = y;
        }
        const int dx = x - start_x;
        if (eligible && y - start_y >= 45 && dx >= -85 && dx <= 85) {
            eligible = false;
            return true;
        }
        return false;
    }
};
class SwipeUp {
    bool active = false, eligible = false;
    int16_t start_x = 0, start_y = 0;
public:
    bool update(bool down, int16_t x, int16_t y) {
        if (!down) { active = false; eligible = false; return false; }
        if (!active) {
            active = true; eligible = y >= 141;
            start_x = x; start_y = y;
        }
        const int dx = x - start_x;
        if (eligible && start_y - y >= 45 && dx >= -85 && dx <= 85) {
            eligible = false; return true;
        }
        return false;
    }
};
enum class QuickAction : uint8_t {
    None, Close, Device, Shutdown, BrightDown, BrightUp,
    VolumeDown, VolumeUp, Shuffle, Repeat
};
inline QuickAction quickMenuAt(bool music, int16_t x, int16_t y) {
    if (x >= 558 && x <= 639 && y >= 0 && y <= 38) return QuickAction::Close;
    if (y < 40 || y > 130) return QuickAction::None;
    if (music) {
        if (x >= 16 && x <= 210 && y >= 80) return x < 113 ? QuickAction::VolumeDown : QuickAction::VolumeUp;
        if (x >= 218 && x <= 412) return QuickAction::Shuffle;
        if (x >= 420 && x <= 614) return QuickAction::Repeat;
    } else {
        if (x >= 16 && x <= 210) return QuickAction::Device;
        if (x >= 218 && x <= 412 && y >= 80) return x < 315 ? QuickAction::BrightDown : QuickAction::BrightUp;
        if (x >= 420 && x <= 614) return QuickAction::Shutdown;
    }
    return QuickAction::None;
}
}
