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

ui::Button pressed_button = ui::Button::None;
bool suppress_press = false;  // set when a touch was consumed by waking the screen

hardware::Battery battery;
uint32_t last_battery_ms = 0;
uint32_t last_frame_ms = 0;
uint32_t last_activity_ms = 0;
bool display_asleep = false;
bool last_seen_playing = false;

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
        if (!suppress_press) pressed_button = ui::hitTest(contact.x, contact.y);
        return;
    }

    // Fire on release, and only if the finger came up over the button it went
    // down on.
    if (!suppress_press) {
        switch (pressed_button) {
            case ui::Button::Previous:  spotify::send(spotify::Command::Previous); break;
            case ui::Button::Next:      spotify::send(spotify::Command::Next); break;
            case ui::Button::PlayPause: spotify::send(spotify::Command::TogglePlayback); break;
            case ui::Button::None:      break;
        }
    }
    pressed_button = ui::Button::None;
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
    handleTouch();

    const uint32_t now = millis();

    if (now - last_battery_ms >= kBatteryIntervalMs) {
        last_battery_ms = now;
        battery = hardware::readBattery();
    }

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

    ui::render(snapshot, battery, pressed_button);
    screen::endFrame();
}
