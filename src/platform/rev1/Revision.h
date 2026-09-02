#pragma once
// Waveshare ESP32-S3-Touch-LCD-3.49 rev1.
// Note the backlight and touch-IRQ pins are SWAPPED relative to rev2, and rev1
// needs -DESP32QSPI_SPI_HOST=SPI3_HOST.
namespace revision {
constexpr int kBacklightPin        = 8;
constexpr int kDisplayGpioResetPin = 21;
constexpr int kTouchIrqPin         = 42;
constexpr bool kResetViaExpander   = false;
constexpr const char *kName        = "rev1";
}  // namespace revision
