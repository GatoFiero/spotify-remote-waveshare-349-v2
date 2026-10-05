#pragma once

#include <Arduino.h>
#include <stdint.h>

namespace app {
enum class LikeState : uint8_t { Ready, Saving, Saved, NeedsAuthorization, Failed };

enum class Status : uint8_t {
    Booting,
    SetupRequired, // Complete the local browser setup before connecting.
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
    bool shuffle = false;
    uint8_t repeat_mode = 0; // off, context, track
    int16_t volume_percent = -1;
    bool supports_volume = false;
    bool device_restricted = false;
    char device_id[64] = {};
    bool control_pending = false;
    int16_t control_result = 0; // last command HTTP status; 0 means none yet
    uint8_t control_kind = 0;

    char title[128] = {};
    char artist[128] = {};
    char album[128] = {};
    char track_uri[64] = {}; // Empty for episodes and local files.
    LikeState like_state = LikeState::Ready;

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
