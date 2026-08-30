# Spotify remote — Waveshare ESP32-S3-Touch-LCD-3.49

A now-playing display and transport remote on the 640 × 172 touch panel. Shows the
current track, artist, album, album art and progress; the three touch buttons do
previous / play-pause / next against whatever device your Spotify account is
currently playing on.

```
┌──────────────────────────────────────────────────────────────────┐
│ ┌────────┐  Bohemian Rhapsody - Rem…                             │
│ │        │  Queen                              ⏮   ▶   ⏭        │
│ │  art   │  A Night at the Opera                                 │
│ │        │  ━━━━━━━━━━──────────────                             │
│ └────────┘  2:14                5:55                        78%  │
└──────────────────────────────────────────────────────────────────┘
```

## Why WiFi and not Bluetooth

The ESP32-S3 has no Bluetooth Classic radio — BLE only. Track metadata over
Bluetooth comes from AVRCP, which is a Classic profile, so a BLE build could send
media-key presses but could never know what is playing. Everything here goes over
WiFi to the Spotify Web API instead, which covers both directions.

Consequence: the board needs a **2.4 GHz** network (there is no 5 GHz radio) and
an internet connection. Control is account-wide, not phone-local — it drives
whichever device is active, which also means it works when your phone is in
another room.

## Setup

**1. Create a Spotify app.** At <https://developer.spotify.com/dashboard>, create
an app and add exactly this redirect URI:

```
http://127.0.0.1:8888/callback
```

Spotify rejects `localhost`; it has to be the literal loopback IP. Note the
client ID. There is no client secret to copy — this uses the PKCE flow precisely
so that no secret has to live in the firmware.

**2. Authorise once, on this machine.**

```bash
python3 tools/spotify_auth.py --client-id <your client id> --write-secrets
```

It opens your browser, you approve, and it writes `src/Secrets.h` with your WiFi
credentials and the refresh token. `src/Secrets.h` is git-ignored. Without
`--write-secrets` it just prints the token for you to paste in yourself.

**3. Build and flash.** Pick the environment matching your board revision —
getting this wrong gives you a board that boots, mounts everything, logs no
errors, and shows a black screen:

```bash
pio run -e rev2 -t upload && pio device monitor   # ESP32-S3-Touch-LCD-3.49B
pio run -e rev1 -t upload && pio device monitor   # original 3.49
```

## Layout of the source

```
src/platform/    pins, rails, reset order, bus choice. Board.h is the only file
                 with GPIO numbers in it; rev1/ and rev2/ differ by three lines.
src/drivers/     TCA9554 expander, AXS15231B touch. Chip facts, no board facts.
src/app/         Spotify client, album art, UI. Never sees a GPIO number.
src/fonts/       generated — see tools/gen_font.py
```

Two tasks: **core 0** runs everything network (WiFi, TLS, JSON, album art
download and decode), **core 1** runs the UI loop. A 500 ms HTTPS round trip
therefore never stalls touch or drawing.

## Things worth knowing

**Refresh tokens rotate.** Under PKCE, Spotify usually issues a new refresh token
on every refresh. The firmware writes the newest one to NVS and only falls back
to the value in `Secrets.h` when NVS is empty — so re-flashing is fine, and you
only need to re-run the auth tool if you revoke access or erase NVS.

**TLS is properly validated.** The ESP-IDF root certificate bundle is already
compiled into the Arduino libs; `net::secure()` attaches it. There is no
`setInsecure()` anywhere.

**Poll rate.** State is fetched every 3 s and the progress bar is interpolated
locally in between, so the bar moves smoothly on one request per 3 s. After a
button press it re-polls after 600 ms, since Spotify takes a moment to settle.

**The screen blanks after two idle minutes,** but only when nothing is playing —
see `kIdleTimeoutMs` in `main.cpp`. It blanks the panel rather than sleeping the
CPU, so polling continues and waking is a repaint from the retained framebuffer,
not a re-render.

**Fonts are generated, not vendored.** `tools/gen_font.py` rasterises a TTF into
an Adafruit-GFX `GFXfont` header. The committed ones come from DejaVu Sans
(permissively licensed, shipped with matplotlib). To change the type:

```bash
python3 tools/gen_font.py /path/to/Font.ttf 22 FontTitle src/fonts/FontTitle.h
```

They cover Latin-1, which handles most accented artist names. Anything outside it
(CJK, emoji) folds to `?` — `src/app/Text.cpp` maps the typographic punctuation
Spotify metadata is full of onto ASCII first.

**Album art** is fetched at 300 × 300 and decoded at half scale to exactly 150 ×
150 into PSRAM. A cover is only drawn when its generation matches the track on
screen, so you never see the previous track's art against a new title.

## If something is wrong

| Symptom | Cause |
| --- | --- |
| Black screen, no errors in the log | Wrong revision environment. Try the other one. |
| "Sign-in expired" on screen | Refresh token rejected; re-run `tools/spotify_auth.py`. |
| "Nothing playing" | No active Spotify session. Press play on the board — it wakes the last device it saw. |
| "Nothing playing" and play does nothing | No controllable device is visible to Spotify at all. Open Spotify on a phone or speaker once so it registers, then try again. |
| BOOT shows "No devices visible to Spotify" | Same cause — nothing is currently registered with your account. |
| Taps land on the wrong button | Touch axis mapping. Add `-DTOUCH_FLIP_X` and/or `-DTOUCH_FLIP_Y`; `-DTOUCH_DEBUG` logs raw and mapped coordinates. |
| Whole UI is upside down | `-DUI_ROTATION=3` (the other landscape orientation). |
| Tearing or corrupt rows | Drop `kBusSpeedHz` in `Board.h` from 40 MHz to 32 MHz. |
| No album art, "no art" tile | Check the log for the HTTP status; art failures retry once, then give up until the next track. |

## Choosing which device to play on

**Press the BOOT button** to open a full-screen list of every device Spotify can
currently see. Tap one and playback moves there; that device also becomes the one
the board wakes automatically from then on.

- A **filled green dot** marks the device currently playing.
- A **green ring** marks the device the board wakes by default.
- Devices marked `not controllable` (`is_restricted`) are shown greyed out and
  cannot be picked -- Spotify lists them but rejects Web API control.
- With more than three devices, **press BOOT again** to page through; the header
  shows `1/2` and so on.
- The overlay closes on the X, on picking a device, or after 20 idle seconds.

Selecting a device keeps playback in whatever state it was: playing carries on
at the new device, and picking from an idle screen starts it there.

The list is fetched live each time it opens rather than accumulated over time. A
device Spotify cannot currently see cannot be transferred to either, so a
remembered-but-absent entry would only be something to tap and have fail.

Note that BOOT is GPIO0, the bootloader strap pin. Pressing it while the board is
running is just a button; holding it down *while resetting* puts the board into
firmware download mode.

## Waking an idle device

Spotify drops a paused session after a while and stops reporting any active
device, which normally leaves a remote with nothing to command. The board keeps
the id and name of the last device it saw playing — in NVS, so it survives a
power cycle — and when you press a button with no live session it first sends
`PUT /me/player` to move playback back to that device, then sends the command.
Pressing play needs nothing further, since transferring with `play: true` is
itself the play action.

If the remembered device has gone (phone off, speaker unplugged), it falls back
to `GET /me/player/devices` and picks the best controllable one: an already
active device first, then one matching the remembered name, then anything that
is not `is_restricted`. Restricted devices appear in the list but reject Web API
control, so they are never chosen.

The idle screen names what it will wake — "Tap play to resume on Kitchen" —
rather than just saying nothing is playing.

## Possible extensions

Volume and shuffle/repeat are two more Web API endpoints (`/volume`,
`/shuffle`, `/repeat`) and would fit naturally as a second page reached by a
swipe. Seeking by tapping the progress bar is `PUT /me/player/seek` plus a hit
test on the bar rectangle.
