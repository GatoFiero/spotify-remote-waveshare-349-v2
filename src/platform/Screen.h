#pragma once
// The 640x172 landscape drawing surface: a full framebuffer in PSRAM, flushed
// at most once per frame.

#include <Arduino_GFX_Library.h>
#include <stdint.h>

// 1 and 3 are the two landscape orientations; they differ by a 180-degree flip.
// Flip this if the board is mounted the other way up.
#ifndef UI_ROTATION
#define UI_ROTATION 1
#endif

namespace screen {

constexpr int16_t kWidth  = 640;
constexpr int16_t kHeight = 172;

// Brings up the QSPI bus, the panel and the PSRAM canvas, pushes one cleared
// frame, and only then turns on the backlight -- powering the panel rail before
// the controller is initialised shows a frame of garbage on every boot.
bool begin();

Arduino_GFX &gfx();

// Marks a logical rectangle as changed. Draw helpers below do this for you; call
// it directly only when drawing through gfx() by hand.
void markDirty(int16_t x, int16_t y, int16_t w, int16_t h);
void markAllDirty();

// Pushes the changed rows, if any. Exactly one of these per frame: a per-widget
// flush turns one 220 KB transfer into fifty.
void endFrame();

// Repaints the whole retained framebuffer, then brings the backlight back.
void wake(uint8_t brightness_percent);
void sleep();

// --- text -----------------------------------------------------------------
// `y` is the text baseline, matching Arduino_GFX's custom-font convention.

uint16_t measure(const GFXfont *font, const char *text);

// Draws `text`, truncating with a trailing ellipsis if it would exceed max_width.
void drawText(const GFXfont *font, int16_t x, int16_t baseline, uint16_t color,
              const char *text, int16_t max_width);

void drawTextRight(const GFXfont *font, int16_t right_x, int16_t baseline, uint16_t color,
                   const char *text);

// --- shapes ---------------------------------------------------------------
void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color);
void drawBitmapRgb565(int16_t x, int16_t y, const uint16_t *pixels, int16_t w, int16_t h);

}  // namespace screen
