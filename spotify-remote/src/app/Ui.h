#pragma once

#include "../platform/Hardware.h"
#include "NowPlaying.h"
#include "SpotifyClient.h"

namespace ui {

enum class Button : uint8_t { None, Previous, PlayPause, Next };

void begin();

// Which button, if any, is under a logical screen coordinate.
Button hitTest(int16_t x, int16_t y);

// Repaints whatever changed. Cheap to call every loop: it diffs against the last
// rendered state and touches nothing if nothing moved.
void render(const app::NowPlaying &now, const hardware::Battery &battery, Button pressed);

// --- device picker --------------------------------------------------------
// A full-screen overlay listing the devices Spotify can currently see, opened
// with the BOOT button.

constexpr int kPickerNone = -1;
constexpr int kPickerClose = -2;

void openPicker();
void closePicker();
bool pickerOpen();

// Advances to the next screenful when more devices are visible than fit.
void nextPickerPage();

// Returns an absolute index into the device list, or kPickerClose / kPickerNone.
int pickerRowAt(const spotify::DeviceList &list, int16_t x, int16_t y);

void renderPicker(const spotify::DeviceList &list, int pressed_index);

// --- power ----------------------------------------------------------------

// Overlay shown while PWR is held, filling as the hold completes so the press
// is both visible and cancellable.
void drawPowerPrompt(uint8_t percent);

// Discards the cached render state so the next render() or renderPicker()
// repaints everything. Used after an overlay has covered the screen.
void invalidate();

}  // namespace ui
