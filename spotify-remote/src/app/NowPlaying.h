#pragma once

#include <Arduino.h>
#include <stdint.h>

namespace app {

enum class Status : uint8_t {
    Booting,
    WifiConnecting,
    Authorizing,
    Playing,        // talking to Spotify successfully (playing or paused)
    NoActiveDevice, // Spotify is reachable but nothing is playing anywhere
    AuthFailed,     // the refresh token was rejected; re-run tools/spotify_auth.py
    NetworkError,
};

struct NowPlaying {
    Status status = Status::Booting;
    bool has_track = false;
    bool is_playing = false;

    char title[128] = {};
    char artist[128] = {};
    char album[128] = {};

    // The device playing, or the last one seen if playback has since gone idle.
    char device_name[64] = {};

    uint32_t duration_ms = 0;

    // progress_ms was true at progress_at_ms (a millis() stamp). The UI
    // interpolates between polls rather than waiting for the next one.
    uint32_t progress_ms = 0;
    uint32_t progress_at_ms = 0;

    // Bumped whenever the track identity changes, so the UI knows to repaint the
    // text block and the art loader knows to fetch a new cover.
    uint32_t track_generation = 0;

    uint32_t elapsedMs() const {
        if (!has_track) return 0;
        uint32_t elapsed = progress_ms;
        if (is_playing) elapsed += millis() - progress_at_ms;
        return elapsed > duration_ms ? duration_ms : elapsed;
    }
};

}  // namespace app
