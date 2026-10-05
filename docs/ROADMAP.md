# Upgrade roadmap

These are proposed additions, not claims about implemented behavior.

1. **Expand touchscreen checks.** The playlist fix has user acceptance on V2. Exercise every menu target and different gesture speeds, then tune thresholds from real use. Keep the requested two-column layout and full-width final shortcut.
2. **Edit playlists without recompiling.** Add a local settings page for Spotify share links, validate them, fetch public titles when available, and store a bounded list in NVS. Provide backup/export without credentials.
3. **Show request progress and useful failures.** Distinguish offline, expired authorization, unsupported device controls, and rate limiting. Retry transient failures conservatively; never report a save as successful before Spotify accepts it.
4. **Seek and sleep timer.** Add progress-bar seeking, then a timer that pauses the active device before blanking the screen. Show whether pause was confirmed rather than assuming it happened.
5. **Correct saved-song state.** Query whether the current track is already in the library, with an explicit permission upgrade if needed. Preserve additive saving unless a user deliberately enables unlike actions.
6. **Reconnect and endurance checks.** Exercise router interruption, device switching, expired/rotated tokens, artwork changes, and extended playback. Measure memory and reset causes on the board.
7. **Safer distribution.** Resolve the upstream license, expand automated checks, and consider a setup flow that provisions credentials after flashing so generic release binaries can be distributed safely.
8. **Speaker experiment.** First verify the codec/amplifier with a test tone. A PC-to-board audio relay would require a running PC and separate receiver software. Native Spotify Connect playback is not supplied by this remote's Web API implementation.
