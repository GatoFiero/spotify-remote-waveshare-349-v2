#include "Hardware.h"

#include <Wire.h>
#include <algorithm>

#include "../drivers/Tca9554.h"
#include "Board.h"

namespace hardware {
namespace {

using namespace board;

constexpr uint8_t kMinimumDuty = 102;  // below roughly this the panel is dark, not dim

void configureBus(TwoWire &wire, int sda, int scl) {
    wire.begin(sda, scl, kI2cClockHz);
    wire.setTimeOut(kI2cTimeoutMs);
}

bool setExpanderPin(uint8_t pin, bool high) {
    return tca9554::configureOutputPin(Wire1, expander::kAddress, pin, high);
}

}  // namespace

bool beginBuses() {
    pinMode(buttons::kBootPin, INPUT_PULLUP);
    pinMode(buttons::kPowerPin, INPUT_PULLUP);
    pinMode(revision::kTouchIrqPin, INPUT_PULLUP);

    // HIGH is off on this inverted backlight; park it there before the rail comes up.
    pinMode(revision::kBacklightPin, OUTPUT);
    digitalWrite(revision::kBacklightPin, HIGH);

    configureBus(Wire, touch_wiring::kSdaPin, touch_wiring::kSclPin);
    configureBus(Wire1, system_bus::kSdaPin, system_bus::kSclPin);

    // The single most important line: on battery the board stays on only while
    // SYS_EN is high.
    return setExpanderPin(expander::kSysEn, true);
}

void releaseDisplayReset() {
    if constexpr (revision::kResetViaExpander) {
        setExpanderPin(expander::kLcdReset, true);
    } else {
        pinMode(revision::kDisplayGpioResetPin, OUTPUT);
        digitalWrite(revision::kDisplayGpioResetPin, HIGH);
    }
    delay(20);
}

void setBacklightRail(bool on) { setExpanderPin(expander::kBacklightRail, on); }

void setBacklightPercent(uint8_t percent) {
    if (percent == 0) {
        pinMode(revision::kBacklightPin, OUTPUT);
        digitalWrite(revision::kBacklightPin, HIGH);  // inverted: HIGH is off
        return;
    }
    percent = std::min<uint8_t>(percent, 100);
    const uint8_t duty = kMinimumDuty + (percent - 1) * (255 - kMinimumDuty) / 99;

    analogWriteResolution(revision::kBacklightPin, 8);
    analogWriteFrequency(revision::kBacklightPin, 25000);  // 1 kHz makes some panels whine
    analogWrite(revision::kBacklightPin, 255 - duty);      // inverted: 0 is full brightness
}

bool touchDataReady() {
    bool high = true;
    if (!tca9554::readInputPin(Wire1, expander::kAddress, expander::kTouchIrq, high)) return false;
    return !high;  // active low
}

void clearTouchInterruptLatch() {
    uint8_t discard = 0;
    tca9554::readRegister(Wire1, expander::kAddress, 0x00, discard);
}

bool powerButtonPressed() { return digitalRead(buttons::kPowerPin) == LOW; }
bool bootButtonPressed() { return digitalRead(buttons::kBootPin) == LOW; }

Battery readBattery() {
    // A single ADC read swings +/-100 mV, which reads as a battery bouncing
    // between 60 % and 80 %. Oversample, trim the outliers, then average.
    constexpr int kSamples = 24;
    constexpr int kDiscardFirst = 2;
    constexpr int kTrimEachEnd = 2;

    // A pin only becomes an ADC channel on its first read, so setting the
    // attenuation before that is a no-op that logs an error. (The core's global
    // default is already 11 dB, so readings were right either way.)
    (void)analogRead(power::kBatteryAdcPin);
    analogSetPinAttenuation(power::kBatteryAdcPin, ADC_11db);
    delay(12);  // let the divider settle

    uint32_t readings[kSamples] = {};
    int count = 0;
    bool any_calibrated = false;
    for (int i = 0; i < kSamples + kDiscardFirst; ++i) {
        const uint32_t mv = analogReadMilliVolts(power::kBatteryAdcPin);
        if (mv > 0) any_calibrated = true;
        if (i >= kDiscardFirst) readings[count++] = mv;
    }
    if (!any_calibrated) {  // eFuse calibration unavailable; fall back to raw counts
        for (int i = 0; i < count; ++i) {
            readings[i] = static_cast<uint32_t>(analogRead(power::kBatteryAdcPin)) * 3300u / 4095u;
        }
    }

    std::sort(readings, readings + count);
    uint32_t sum = 0;
    const int kept = count - 2 * kTrimEachEnd;
    for (int i = kTrimEachEnd; i < count - kTrimEachEnd; ++i) sum += readings[i];

    Battery battery;
    battery.volts = (static_cast<float>(sum) / kept) * power::kDividerRatio / 1000.0f;
    battery.present = battery.volts >= 2.5f && battery.volts <= 4.6f;
    if (!battery.present) return battery;

    // The 3.65-3.85 V region is most of the cell's life, so a linear voltage-to-
    // percent map makes the gauge look stuck at 50 % and then plummet.
    static constexpr struct { float volts; uint8_t percent; } kCurve[] = {
        {3.30f, 0},  {3.50f, 5},  {3.60f, 10}, {3.65f, 20}, {3.70f, 30}, {3.75f, 40},
        {3.79f, 50}, {3.85f, 60}, {3.92f, 70}, {4.00f, 80}, {4.10f, 90}, {4.15f, 100},
    };
    constexpr int kPoints = sizeof(kCurve) / sizeof(kCurve[0]);

    if (battery.volts <= kCurve[0].volts) {
        battery.percent = 0;
    } else if (battery.volts >= kCurve[kPoints - 1].volts) {
        battery.percent = 100;
    } else {
        for (int i = 1; i < kPoints; ++i) {
            if (battery.volts > kCurve[i].volts) continue;
            const float span = kCurve[i].volts - kCurve[i - 1].volts;
            const float t = (battery.volts - kCurve[i - 1].volts) / span;
            battery.percent = kCurve[i - 1].percent +
                              static_cast<uint8_t>(t * (kCurve[i].percent - kCurve[i - 1].percent));
            break;
        }
    }
    return battery;
}

bool powerOff() { return setExpanderPin(expander::kSysEn, false); }

const char *revisionName() { return revision::kName; }

}  // namespace hardware
