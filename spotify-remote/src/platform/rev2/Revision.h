#pragma once
// Waveshare ESP32-S3-Touch-LCD-3.49B rev2.
// Selected by -DBOARD_REVISION_HEADER; see platformio.ini.
namespace revision {
constexpr int kBacklightPin        = 42;
constexpr int kDisplayGpioResetPin = -1;  // reset is on the expander, EXIO5
constexpr int kTouchIrqPin         = 8;
constexpr bool kResetViaExpander   = true;
constexpr const char *kName        = "rev2";
}  // namespace revision
