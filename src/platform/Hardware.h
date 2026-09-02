#pragma once
// Board bring-up, backlight, battery and power-off. The app sees only this.

#include <stdint.h>

namespace hardware {

// Brings up both I2C buses and latches the battery power hold. Must run before
// anything slow, or the board dies in your hand when running on battery.
bool beginBuses();

// Releases the panel reset (rev2: expander EXIO5, rev1: a GPIO) and powers the
// backlight rail. Call between panel construction and the first flush.
void releaseDisplayReset();
void setBacklightRail(bool on);

// 1..100 %, mapped onto the usable end of the inverted PWM range. 0 turns the
// PWM fully off without cutting the rail.
void setBacklightPercent(uint8_t percent);

// True while the touch controller has a packet waiting (expander EXIO0, active low).
bool touchDataReady();

// Reads the expander input register, clearing any latched touch interrupt. A
// pending interrupt makes light sleep a no-op.
void clearTouchInterruptLatch();

bool powerButtonPressed();
bool bootButtonPressed();

struct Battery {
    bool present = false;
    float volts = 0.0f;
    uint8_t percent = 0;
};
Battery readBattery();

// Clears the SYS_EN latch. On battery this is the clean power-off; on USB the
// board stays up regardless.
bool powerOff();

// The user-facing "turn it off". Clears SYS_EN, which on battery ends execution
// here. If the board is still running afterwards it is on USB, where the latch
// has no effect, so it light-sleeps until PWR is pressed again -- as close to
// off as a board with no PMIC can get. Returns once it has been woken.
//
// The caller owns the display: blank it before calling, repaint after.
void powerDown();

// "rev1" / "rev2", for logging.
const char *revisionName();

}  // namespace hardware
