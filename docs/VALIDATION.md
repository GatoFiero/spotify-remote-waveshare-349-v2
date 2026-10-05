# Validation status

Tested target: Waveshare ESP32-S3-Touch-LCD-3.49 V2, 16 MB flash and 8 MB PSRAM, `rev2` PlatformIO environment.

## Observed on the physical board

- Successful build, upload verification, and restart.
- Wi-Fi connection, Spotify token refresh, real current-track metadata, and track changes.
- Top-left music menu, top-right device menu, and bottom-edge playlist gesture events.
- Spotify accepted multiple volume commands and playlist launches with shuffle.
- Device discovery returned available playback devices.
- Physical tap events for brightness adjustments were detected.
- The user confirmed that the repaired playlist menu is readable and clickable, including the full-width New Releases shortcut and large close control. Live logs recorded multiple shortcut taps followed by successful shuffled-playlist requests.

API acceptance is not the same as audible playback, visible layout correctness, or complete hardware acceptance.

## Software checks

Native C++ checks cover transport target boundaries; playlist row/column gaps and the full-width ninth target; close-button separation; top-left/right and bottom-edge swipes; small finger drift, drag cancellation, and one release per tap; valid track/playlist URIs and rejection of unsupported or malformed inputs.

Public CI builds with the placeholder example configuration. Local configured binaries are private and are never release assets.

## Open acceptance items

- Earlier compact playlist rows were reported as glitchy and difficult to select. The fix enlarges rows, moves request feedback into the header, and retains the initially touched target through small movement. The user has accepted this fix on V2; broader ergonomics testing is still useful.
- Real Liked Songs outcome after a save, shuffle/repeat toggles, and device-transfer behavior across playback targets.
- Battery shutdown/restart, USB sleep/wake, V1 hardware, long-term operation, and controlled reconnect/rate-limit tests.
- Onboard speaker playback is not implemented.
