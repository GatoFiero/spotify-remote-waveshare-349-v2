# Spotify Remote Plus — Waveshare 3.49 V2

A touchscreen Spotify remote for the **Waveshare ESP32-S3-Touch-LCD-3.49 V2**, with a 640 × 172 display, album artwork, gesture menus, Liked Songs saving, and playlist shortcuts.

Built on [FaridZandi/eps32-spotify-controller](https://github.com/FaridZandi/eps32-spotify-controller). The upstream history and author attribution are retained. This upgrade was developed and flashed on the V2 board; the inherited V1 build configuration has not been tested with these additions.

**This is a remote:** music plays on your existing phone, computer, or Spotify Connect speaker. The onboard speaker is not a Spotify playback device in this firmware.

## Controls

| Action | Result |
|---|---|
| Main screen | Artwork, title, artist, album, progress, previous / play-pause / next, and a heart |
| Swipe down from the top left | Volume −/+ in 5% steps, shuffle, and repeat Off / All / One song |
| Swipe down from the top right | Choose playback device, brightness, and shutdown |
| Swipe up from the bottom | Eight playlist shortcuts in two columns, plus a ninth full-width shortcut at the bottom |
| Large X | Close a menu |
| Short PWR press or BOOT | Open the Spotify device picker |
| Hold PWR | Power off on battery; sleep on USB |
| RESET | Restart the board |

Begin a swipe near the screen edge, move about a quarter of the screen height, and release before tapping. Taps keep their initial target through small finger movement; dragging farther cancels the tap. Opening a menu cannot also trigger one of its options.

The heart saves the displayed track to Liked Songs. White means ready, yellow means saving, green means Spotify accepted the save, and red means it failed. It does not remove songs or query whether a song was already liked. Episodes and local files are excluded.

Playlist shortcuts start a Spotify playlist context on the current playback device and request shuffle before and after loading it. Device selection, playlist launch, and music controls depend on Spotify and the target device accepting the request. Errors are shown instead of a false success state.

Brightness changes in 10% steps and persists across restarts. Touchscreen shutdown powers off on battery. USB keeps the processor powered, so touchscreen shutdown blanks the display; a tap wakes it, and Spotify continues running during this USB standby. The physical PWR hold uses the inherited USB sleep behavior.

## Requirements

- Waveshare ESP32-S3-Touch-LCD-3.49 **V2**, a USB data cable, and 2.4 GHz Wi-Fi with internet access.
- Spotify Premium for playback-control APIs and a Spotify developer app you can authorize.
- Python 3.10 or newer, Git, and the tools in `requirements.txt`.
- An available Spotify playback device. Open Spotify on it before testing the remote.

## Set up Spotify and Wi-Fi

1. In the [Spotify developer dashboard](https://developer.spotify.com/dashboard), create an app and select **Web API**. Register exactly `http://127.0.0.1:8888/callback` as its redirect URI. No client secret is needed.
2. Clone this repository and install the tools:

   ```sh
   git clone https://github.com/GatoFiero/spotify-remote-waveshare-349-v2.git
   cd spotify-remote-waveshare-349-v2
   python -m pip install -r requirements.txt
   python tools/local_setup.py
   ```

3. Open [the local setup page](http://127.0.0.1:8888/) on the same computer. Enter your app Client ID and Wi-Fi details, then approve Spotify authorization. The server binds only to loopback, uses PKCE and form protection, and writes `src/Secrets.h` locally.
4. Connect the board, build, and upload:

   ```sh
   python -m platformio run -e rev2
   python -m platformio device list
   python -m platformio run -e rev2 -t upload --upload-port YOUR_SERIAL_PORT
   ```

   Replace `YOUR_SERIAL_PORT` with the board's port, such as `COM6` on Windows or `/dev/ttyACM0` on Linux. Close serial monitors before uploading.

On Windows, after installing the requirements, `powershell -ExecutionPolicy Bypass -File tools/setup-windows.ps1 -Port COM6` performs setup, build, upload, and boot diagnostics. Replace the port for your computer. The script also accepts `-Python` and `-SkipBootCheck`.

For command-line authorization instead of the browser form:

```sh
python tools/spotify_auth.py --client-id YOUR_CLIENT_ID --write-secrets
```

Credentials are saved locally and are not printed. Each deliberate authorization gets a version marker so the firmware imports the new refresh token; normal reflashing retains the token rotated by Spotify in NVS.

Use `/wifi` on the local setup server to update Wi-Fi while retaining authorization, or `/reauthorize` to approve the current Liked Songs scope. Rebuild and upload after changing local credentials.

## Customize playlists

Edit `src/app/PlaylistShortcuts.h`. Each entry has a label and a `spotify:playlist:...` URI. For a share link such as `https://open.spotify.com/playlist/PLAYLIST_ID?si=...`, use `spotify:playlist:PLAYLIST_ID`; discard the query string.

The supplied nine shortcuts are examples of the setup used during development. Personalized playlists may depend on account access. Slots 1–8 occupy the two columns; slot 9 spans both columns at the bottom. The current layout supports up to nine shortcuts.

## Validation and limitations

Native C++ checks cover touch-target separation, swipe recognition, tap drift and drag cancellation, and track/playlist URI validation:

```sh
python tools/run_native_tests.py
```

This command needs `g++` on PATH. CI runs the same checks and builds V2 using **placeholder credentials**. The configured firmware has been flashed and observed receiving real Spotify metadata, accepting volume changes, and launching playlists with shuffle. Read [the validation notes](docs/VALIDATION.md) for the boundary between build checks, API acceptance, and physical confirmation.

The project is a working hardware prototype. The playlist tap/layout fix has been confirmed readable and clickable on the V2 board; battery shutdown, long-term reconnect behavior, and every Spotify/device combination are not fully qualified. Build success alone does not prove touchscreen usability or audible playback.

## Protect your credentials

`src/Secrets.h`, local setup state, serial logs, device backups, and generated firmware files are excluded from Git. **Do not upload configured firmware binaries:** they contain Wi-Fi and Spotify credentials. Only source and the placeholder example are published here. Back up an existing device before replacing its firmware; never restore another person's flash backup onto your board.

## Source and next improvements

- `src/platform/`: display, touch, power, and board wiring.
- `src/app/`: Spotify requests, artwork, UI, gestures, and playlist configuration.
- `tools/`: local authorization, setup, diagnostics, and native checks.
- `tests/`: native input and request-policy checks.

See [the roadmap](docs/ROADMAP.md) for playlist editing without rebuilding, clearer request feedback, seeking, a sleep timer, and recovery improvements. Speaker streaming is a separate experiment, not an implemented feature.

See [NOTICE.md](NOTICE.md) for upstream provenance and license status. This project is not affiliated with Spotify or Waveshare.
