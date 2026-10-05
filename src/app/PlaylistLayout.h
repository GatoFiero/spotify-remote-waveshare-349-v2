#pragma once
#include <stdint.h>
namespace ui {
inline int playlistShortcutAt(uint8_t count, int16_t x, int16_t y) {
    if (x >= 558 && x <= 639 && y >= 0 && y <= 38) return -2;
    if (count > 8 && x >= 16 && x <= 624 && y >= 146 && y <= 171) return 8;
    if (y < 40 || y > 141) return -1;
    const int row = (y - 40) / 26;
    if ((y - 40) % 26 > 23) return -1;
    const int column = x >= 16 && x <= 310 ? 0 : x >= 330 && x <= 624 ? 1 : -1;
    if (column < 0) return -1;
    const int index = row * 2 + column;
    return index < count ? index : -1;
}
}
