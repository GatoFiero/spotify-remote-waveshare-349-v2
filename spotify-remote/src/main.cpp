// Spotify now-playing remote for the Waveshare ESP32-S3-Touch-LCD-3.49.
//
// Core 0 runs the Spotify task (WiFi, TLS, JSON, album art). Core 1 runs this
// loop: poll touch, repaint what changed, flush once. Nothing here blocks on the
// network, so the buttons stay responsive while a request is in flight.

#include <Arduino.h>

#include "app/AlbumArt.h"
#include "app/NowPlaying.h"
#include "app/SpotifyClient.h"
#include "app/Ui.h"
#include "platform/Hardware.h"
#include "platform/Screen.h"
#include "platform/Touch.h"

namespace {

constexpr uint8_t kBrightnessPercent = 85;
constexpr uint32_t kBatteryIntervalMs = 30000;
constexpr uint32_t kIdleTimeoutMs = 120000;  // blank the panel after two idle minutes
constexpr uint32_t kFrameIntervalMs = 33;    // ~30 fps ceiling; we redraw far less than that
constexpr uint32_t kPickerTimeoutMs = 20000; // close the picker if it is left open
constexpr uint32_t kBootDebounceMs = 40;
// Held rather than tapped, so a brush against the button does not kill the
// board mid-song. The prompt appears early enough to explain what is happening.
constexpr uint32_t kPowerPromptMs = 200;
constexpr uint32_t kPowerHoldMs = 900;

ui::Button pressed_button = ui::Button::None;
int picker_pressed = ui::kPickerNone;
spotify::DeviceList device_list;
uint32_t picker_active_ms = 0;
bool boot_was_down = false;
uint32_t boot_changed_ms = 0;
uint32_t power_hold_started_ms = 0;
bool power_prompt_visible = false;
int16_t power_prompt_percent = -1;
bool suppress_press = false;  // set when a touch was consumed by waking the screen

hardware::Battery battery;
uint32_t last_battery_ms = 0;
uint32_t last_frame_ms = 0;
uint32_t last_activity_ms = 0;
bool display_asleep = false;
bool last_seen_playing = false;

// PWR held for kPowerHoldMs powers the board down. On battery that is a real
// power-off; on USB it becomes a light sleep that the same button wakes.
void handlePowerButton() {
    const uint32_t now = millis();

    if (!hardware::powerButtonPressed()) {
        if (power_prompt_visible) {  // released early: put the screen back
            ui::invalidate();
            power_prompt_visible = false;
            power_prompt_percent = -1;
        }
        power_hold_started_ms = 0;
        return;
    }

    if (power_hold_started_ms == 0) power_hold_started_ms = now;
    last_activity_ms = now;
    const uint32_t held = now - power_hold_started_ms;

    if (held >= kPowerHoldMs) {
        log_i("power button held: shutting down");
        screen::sleep();
        hardware::powerDown();  // returns only on USB, once PWR is pressed again

        // Repaint a correct frame before the backlight comes back, or the first
        // thing the user sees is the shutdown screen again.
        ui::invalidate();
        power_prompt_visible = false;
        power_prompt_percent = -1;
        power_hold_started_ms = 0;
        display_asleep = false;

        app::NowPlaying resumed;
        spotify::snapshot(resumed);
        ui::render(resumed, battery, ui::Button::None);
        screen::wake(kBrightnessPercent);
        last_activity_ms = millis();
        return;
    }

    if (held < kPowerPromptMs) return;

    // Redraw only when the bar actually moves; this overlay is a full-screen
    // repaint and does not need to run at frame rate.
    const int16_t percent = held * 100 / kPowerHoldMs;
    if (percent / 5 == power_prompt_percent / 5) return;
    power_prompt_percent = percent;
    power_prompt_visible = true;
    ui::drawPowerPrompt(percent);
    screen::endFrame();
}

// BOOT opens the device picker; pressing it again pages through the list when
// there are more devices than fit on one screen.
void handleBootButton() {
    const bool down = hardware::bootButtonPressed();
    const uint32_t now = millis();

    if (down != boot_was_down) {
        if (now - boot_changed_ms < kBootDebounceMs) return;  // contact bounce
        boot_changed_ms = now;
        boot_was_down = down;

        if (down) {
            last_activity_ms = now;
            picker_active_ms = now;
            if (display_asleep) {
                screen::wake(kBrightnessPercent);
                display_asleep = false;
            } else if (!ui::pickerOpen()) {
                ui::openPicker();
                spotify::send(spotify::Command::RefreshDevices);
            } else {
                ui::nextPickerPage();
            }
        }
    }
}

void handleTouch() {
    const touch::Contact &contact = touch::current();

    if (contact.down) {
        last_activity_ms = millis();
        if (display_asleep) {  // the first touch after blanking only wakes the screen
            screen::wake(kBrightnessPercent);
            display_asleep = false;
            suppress_press = true;
            return;
        }
        // Tracked every poll, so sliding off a button clears it and cancels.
        if (suppress_press) return;
        if (ui::pickerOpen()) {
            picker_active_ms = millis();
            picker_pressed = ui::pickerRowAt(device_list, contact.x, contact.y);
        } else {
            pressed_button = ui::hitTest(contact.x, contact.y);
        }
        return;
    }

    // Fire on release, and only if the finger came up over the button it went
    // down on.
    if (!suppress_press) {
        if (ui::pickerOpen()) {
            if (picker_pressed == ui::kPickerClose) {
                ui::closePicker();
            } else if (picker_pressed >= 0 && picker_pressed < device_list.count) {
                spotify::selectDevice(device_list.items[picker_pressed].id);
                ui::closePicker();
            }
        } else {
            switch (pressed_button) {
                case ui::Button::Previous:  spotify::send(spotify::Command::Previous); break;
                case ui::Button::Next:      spotify::send(spotify::Command::Next); break;
                case ui::Button::PlayPause: spotify::send(spotify::Command::TogglePlayback); break;
                case ui::Button::None:      break;
            }
        }
    }
    pressed_button = ui::Button::None;
    picker_pressed = ui::kPickerNone;
    suppress_press = false;
}

}  // namespace

void setup() {
    Serial.begin(115200);

    if (!hardware::beginBuses()) {
        log_e("I2C bring-up failed; the expander did not answer");
    }
    if (!screen::begin()) {
        log_e("display bring-up failed");
    }
    ui::begin();
    screen::endFrame();
    hardware::setBacklightPercent(kBrightnessPercent);

    if (!touch::begin()) log_w("touch controller did not answer at 0x3B");
    if (!albumart::begin()) log_w("album art disabled (no PSRAM)");
    if (!spotify::begin()) log_e("could not start the Spotify task");

    battery = hardware::readBattery();
    last_battery_ms = last_activity_ms = millis();
    log_i("ready: %s, %s", hardware::revisionName(), battery.present ? "on battery" : "on USB");
}

void loop() {
    touch::poll();
    handlePowerButton();
    handleBootButton();
    handleTouch();

    // While the shutdown prompt is up it owns the screen; rendering the normal
    // view underneath would fight with it.
    if (power_prompt_visible) return;

    const uint32_t now = millis();

    // An overlay left open forever would hide the thing the board is for.
    if (ui::pickerOpen() && now - picker_active_ms > kPickerTimeoutMs) ui::closePicker();

    if (now - last_battery_ms >= kBatteryIntervalMs) {
        last_battery_ms = now;
        battery = hardware::readBattery();
    }

    if (ui::pickerOpen()) last_activity_ms = now;  // never blank mid-selection

    // No PMIC here means no charge or VBUS sense, so "idle" is the only signal we
    // have. Blank the panel rather than sleeping the CPU: the Spotify task keeps
    // polling, so the screen is already correct the instant it comes back.
    if (!display_asleep && !last_seen_playing && now - last_activity_ms > kIdleTimeoutMs) {
        screen::sleep();
        display_asleep = true;
    }

    if (now - last_frame_ms < kFrameIntervalMs) return;
    last_frame_ms = now;

    app::NowPlaying snapshot;
    spotify::snapshot(snapshot);
    last_seen_playing = snapshot.is_playing;

    if (display_asleep) {
        // Keep the framebuffer current so waking is a repaint, not a re-render.
        ui::render(snapshot, battery, ui::Button::None);
        return;
    }

    if (ui::pickerOpen()) {
        spotify::deviceSnapshot(device_list);
        ui::renderPicker(device_list, picker_pressed);
    } else {
        ui::render(snapshot, battery, pressed_button);
    }
    screen::endFrame();
}
