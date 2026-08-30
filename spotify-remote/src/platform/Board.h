#pragma once
// Every GPIO number and I2C address for this board family lives here and
// nowhere else. Nothing above src/platform/ includes this file.

#include <Arduino.h>

#ifndef BOARD_REVISION_HEADER
#error "Build env must define BOARD_REVISION_HEADER (see platformio.ini)."
#endif
#include BOARD_REVISION_HEADER

namespace board {

namespace display_wiring {
constexpr int8_t kCs   = 9;
constexpr int8_t kSclk = 10;
constexpr int8_t kD0   = 11;
constexpr int8_t kD1   = 12;
constexpr int8_t kD2   = 13;
constexpr int8_t kD3   = 14;
constexpr int32_t kBusSpeedHz = 40'000'000;  // library documents 32 MHz; 40 is stable here
constexpr int16_t kPanelWidth  = 172;        // native (portrait) geometry
constexpr int16_t kPanelHeight = 640;
}  // namespace display_wiring

namespace touch_wiring {
constexpr uint8_t kI2cAddress = 0x3B;
constexpr int kSdaPin = 17;
constexpr int kSclPin = 18;
}  // namespace touch_wiring

namespace system_bus {
constexpr int kSdaPin = 47;
constexpr int kSclPin = 48;
}  // namespace system_bus

constexpr uint32_t kI2cClockHz   = 300'000;
constexpr uint16_t kI2cTimeoutMs = 10;  // the Arduino default of 50 ms shows up as frame hitches

namespace expander {
constexpr uint8_t kAddress       = 0x20;
constexpr uint8_t kTouchIrq      = 0;  // input, active low
constexpr uint8_t kBacklightRail = 1;
constexpr uint8_t kLcdReset      = 5;  // rev2 only
constexpr uint8_t kSysEn         = 6;  // battery power hold latch
constexpr uint8_t kAudioRail     = 7;
}  // namespace expander

namespace buttons {
constexpr int kBootPin = 0;
constexpr int kPowerPin = 16;  // also the light-sleep wake source
}  // namespace buttons

namespace power {
constexpr int kBatteryAdcPin = 4;
constexpr float kDividerRatio = 3.0f;
}  // namespace power

}  // namespace board
