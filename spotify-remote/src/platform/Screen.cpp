#include "Screen.h"

#include <esp_heap_caps.h>

#include <algorithm>

#include "Board.h"
#include "Hardware.h"

namespace screen {
namespace {

using namespace board::display_wiring;

// Arduino_Canvas allocates its framebuffer with plain aligned_alloc, which lands
// in internal RAM -- and 172 * 640 * 2 = 220,160 bytes does not fit there. Claim
// the buffer from PSRAM before the base class gets a chance to try.
class PsramCanvas : public Arduino_Canvas {
   public:
    using Arduino_Canvas::Arduino_Canvas;

    bool begin(int32_t speed = GFX_NOT_DEFINED) override {
        if (!_framebuffer) {
            const size_t bytes = static_cast<size_t>(kPanelWidth) * kPanelHeight * 2;
            _framebuffer = static_cast<uint16_t *>(
                heap_caps_aligned_alloc(16, bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
            if (!_framebuffer) {
                log_e("no PSRAM for the %u-byte framebuffer", bytes);
                return false;
            }
        }
        return Arduino_Canvas::begin(speed);
    }
};

Arduino_ESP32QSPI *bus = nullptr;
Arduino_AXS15231B *panel = nullptr;
PsramCanvas *canvas = nullptr;

struct Dirty {
    int16_t left = 0, top = 0, right = -1, bottom = -1;
    bool empty() const { return right < left; }
    void clear() { left = top = 0; right = bottom = -1; }
    void add(int16_t x, int16_t y, int16_t w, int16_t h) {
        if (w <= 0 || h <= 0) return;
        const int16_t x2 = x + w - 1, y2 = y + h - 1;
        if (empty()) {
            left = x; top = y; right = x2; bottom = y2;
        } else {
            left = std::min(left, x);   top = std::min(top, y);
            right = std::max(right, x2); bottom = std::max(bottom, y2);
        }
        left = std::max<int16_t>(left, 0);
        top = std::max<int16_t>(top, 0);
        right = std::min<int16_t>(right, kWidth - 1);
        bottom = std::min<int16_t>(bottom, kHeight - 1);
    }
};
Dirty dirty;

// How many native panel rows have to be pushed to cover the dirty region.
//
// Arbitrary CASET/RASET windows on this panel are fussy, so we never change the
// column window: we always send full-width rows starting at row 0 and only
// shorten the row count. A change at one end of the screen costs a fraction of a
// frame; a change at the other costs a full one.
int16_t dirtyRowCount() {
    if (dirty.empty()) return 0;
#if UI_ROTATION == 1
    return std::min<int16_t>(dirty.right + 1, kPanelHeight);   // native row == logical x
#elif UI_ROTATION == 3
    return std::min<int16_t>(kPanelHeight - dirty.left, kPanelHeight);  // native row == 639 - x
#else
#error "UI_ROTATION must be 1 or 3 (the two landscape orientations)."
#endif
}

const GFXglyph *glyphFor(const GFXfont *font, uint8_t c) {
    if (c < font->first || c > font->last) return nullptr;
    return &font->glyph[c - font->first];
}

}  // namespace

bool begin() {
    bus = new Arduino_ESP32QSPI(kCs, kSclk, kD0, kD1, kD2, kD3, false);
    panel = new Arduino_AXS15231B(bus, revision::kDisplayGpioResetPin, /*rotation=*/0,
                                  /*ips=*/false, kPanelWidth, kPanelHeight, 0, 0, 0, 0);

    hardware::releaseDisplayReset();

    canvas = new PsramCanvas(kPanelWidth, kPanelHeight, panel, 0, 0, UI_ROTATION);
    if (!canvas->begin(kBusSpeedHz)) return false;

    canvas->setTextWrap(false);  // we truncate; wrapping would spill onto a second line
    canvas->fillScreen(RGB565_BLACK);
    markAllDirty();
    endFrame();

    hardware::setBacklightRail(true);
    return true;
}

Arduino_GFX &gfx() { return *canvas; }

void markDirty(int16_t x, int16_t y, int16_t w, int16_t h) { dirty.add(x, y, w, h); }
void markAllDirty() { dirty.add(0, 0, kWidth, kHeight); }

void endFrame() {
    const int16_t rows = dirtyRowCount();
    if (rows <= 0) return;
    panel->draw16bitRGBBitmap(0, 0, canvas->getFramebuffer(), kPanelWidth, rows);
    dirty.clear();
}

void wake(uint8_t brightness_percent) {
    markAllDirty();
    endFrame();
    hardware::setBacklightRail(true);
    hardware::setBacklightPercent(brightness_percent);
}

void sleep() {
    hardware::setBacklightPercent(0);
    hardware::setBacklightRail(false);
}

uint16_t measure(const GFXfont *font, const char *text) {
    uint16_t width = 0;
    for (const uint8_t *p = reinterpret_cast<const uint8_t *>(text); *p; ++p) {
        if (const GFXglyph *g = glyphFor(font, *p)) width += g->xAdvance;
    }
    return width;
}

void drawText(const GFXfont *font, int16_t x, int16_t baseline, uint16_t color,
              const char *text, int16_t max_width) {
    canvas->setFont(font);
    canvas->setTextSize(1);
    canvas->setTextColor(color);
    canvas->setCursor(x, baseline);

    const uint16_t full = measure(font, text);
    if (full <= max_width) {
        canvas->print(text);
        markDirty(x, baseline - font->yAdvance, full + 2, font->yAdvance * 2);
        return;
    }

    // Truncate to fit, leaving room for a trailing ellipsis.
    const uint16_t ellipsis = measure(font, "...");
    uint16_t used = 0;
    const uint8_t *p = reinterpret_cast<const uint8_t *>(text);
    for (; *p; ++p) {
        const GFXglyph *g = glyphFor(font, *p);
        if (!g) continue;
        if (used + g->xAdvance + ellipsis > max_width) break;
        used += g->xAdvance;
        canvas->write(*p);
    }
    canvas->print("...");
    markDirty(x, baseline - font->yAdvance, used + ellipsis + 2, font->yAdvance * 2);
}

void drawTextRight(const GFXfont *font, int16_t right_x, int16_t baseline, uint16_t color,
                   const char *text) {
    const uint16_t width = measure(font, text);
    drawText(font, right_x - width, baseline, color, text, width + 1);
}

void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    canvas->fillRect(x, y, w, h, color);
    markDirty(x, y, w, h);
}

void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color) {
    canvas->fillRoundRect(x, y, w, h, r, color);
    markDirty(x, y, w, h);
}

void drawBitmapRgb565(int16_t x, int16_t y, const uint16_t *pixels, int16_t w, int16_t h) {
    canvas->draw16bitRGBBitmap(x, y, const_cast<uint16_t *>(pixels), w, h);
    markDirty(x, y, w, h);
}

}  // namespace screen
