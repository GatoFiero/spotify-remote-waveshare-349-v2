#pragma once
#include <stdint.h>
namespace app {
struct PlaylistShortcut { const char *label; const char *uri; };
// Filled with the user's chosen public Spotify playlist links.
constexpr PlaylistShortcut kPlaylistShortcuts[] = {
    {"REGGAETONTONTON", "spotify:playlist:5KX6VCjZ5JD7vL6D8qBLSI"},
    {"On Repeat", "spotify:playlist:37i9dQZF1EpqnPM1NDDu6a"},
    {"Lo mejor del mes", "spotify:playlist:37i9dQZF1DWZoF06RIo9el"},
    {"hyperpop", "spotify:playlist:37i9dQZF1DX7HOk71GPfSw"},
    {"Release Radar", "spotify:playlist:37i9dQZEVXbp7Qhy0wG4V0"},
    {"hexxed", "spotify:playlist:37i9dQZF1DX7XEgl7z0Lyy"},
    {"Fuego", "spotify:playlist:37i9dQZF1DX8sljIJzI0oo"},
    {"hauntology", "spotify:playlist:37i9dQZF1DX9TOdl0GpvQm"},
    {"New Releases", "spotify:playlist:3MROrfGpz1hlkP7UFPqPO4"},
};
constexpr uint8_t kPlaylistCount = sizeof(kPlaylistShortcuts) / sizeof(kPlaylistShortcuts[0]);
}
