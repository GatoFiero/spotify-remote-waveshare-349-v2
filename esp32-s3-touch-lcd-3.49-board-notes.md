# Waveshare ESP32-S3-Touch-LCD-3.49 — Board Notes

Standalone bring-up and design notes for the **Waveshare ESP32-S3-Touch-LCD-3.49B (Rev2)** and its
Rev1 sibling, distilled from a shipping Arduino/ESP-IDF firmware that runs on both revisions.

This document is self-contained: every pin, register, magic constant, and workaround needed to bring
the board up is reproduced here. It assumes no particular codebase.

---

## 1. What is actually on the board

| Function | Part | Bus | Address / notes |
| --- | --- | --- | --- |
| MCU | ESP32-S3 (N16R8) | — | 16 MB QIO flash, 8 MB **OPI** PSRAM, 240 MHz |
| Display | AXS15231B, 172 × 640 IPS | QSPI | 4-data-line SPI, no separate D/C line |
| Touch | AXS15231B (same die, separate I2C interface) | I2C0 | `0x3B` |
| GPIO expander | TCA9554 | I2C1 | `0x20` |
| IMU | QMI8658 | I2C1 | `0x6B` |
| Audio codec | ES8311 | I2C1 + I2S0 | `0x18` |
| Storage | microSD | SDMMC, **1-bit** | CLK/CMD/D0 only |
| Battery | Li-ion + resistor divider | ADC1 | GPIO4, ÷3 divider |

Two things about the ESP32-S3 module dominate the design:

- **8 MB octal PSRAM.** GPIO 26–37 are consumed by the flash/PSRAM octal bus. Never assign them.
  In exchange you get enough RAM for a full-screen 16-bit framebuffer (see §5).
- **16 MB flash with the stock `default_16MB.csv` layout** gives two 6.25 MB OTA app slots. A
  release build of a fairly large C++23 app lands around 2.9 MB flash / 125 KB internal RAM, so
  there is real headroom.

---

## 2. Complete pin map

### Display (QSPI)

| Signal | GPIO |
| --- | --- |
| CS | 9 |
| SCLK | 10 |
| D0 | 11 |
| D1 | 12 |
| D2 | 13 |
| D3 | 14 |
| RESET | **rev1: GPIO21 — rev2: none (use expander EXIO5)** |
| Backlight PWM | **rev1: GPIO8 — rev2: GPIO42** |

Panel geometry: `172` wide × `640` tall in native/panel orientation. The product is used in
landscape, so the UI surface is `640 × 172`.

### I2C — two separate buses

| Bus | SDA | SCL | Clock | Timeout | Devices |
| --- | --- | --- | --- | --- | --- |
| I2C0 (`Wire`) | 17 | 18 | 300 kHz | 10 ms | AXS15231B touch `0x3B` |
| I2C1 (`Wire1`) | 47 | 48 | 300 kHz | 10 ms | TCA9554 `0x20`, ES8311 `0x18`, QMI8658 `0x6B` |

Keeping touch on its own bus matters: touch is polled at ~30 ms and must not queue behind codec or
expander traffic. Set an explicit `setTimeOut(10)` on both buses — the Arduino default of 50 ms
turns one unacked device into a visible frame hitch.

### Buttons

| Button | GPIO | Polarity |
| --- | --- | --- |
| BOOT | 0 | active low, `INPUT_PULLUP` |
| PWR | 16 | active low, `INPUT_PULLUP` |

GPIO16 is also the light-sleep wake source.

### Touch interrupt

| Rev | Direct GPIO | Also mirrored on |
| --- | --- | --- |
| rev1 | 42 | TCA9554 EXIO0 |
| rev2 | 8 | TCA9554 EXIO0 |

### SD card (SDMMC, 1-bit)

| Signal | GPIO |
| --- | --- |
| CLK | 41 |
| CMD | 39 |
| D0 | 40 |
| D1/D2/D3 | not connected |

### Audio (I2S0 → ES8311)

| Signal | GPIO |
| --- | --- |
| MCLK | 7 |
| BCLK | 15 |
| WS / LRCK | 46 |
| DIN (board net) | 6 |
| DOUT (board net) | 45 |

### Power

| Signal | GPIO |
| --- | --- |
| Battery ADC | 4 (ADC1, 12-bit, 11 dB attenuation, ÷3 divider) |

### TCA9554 expander pin assignment

| EXIO | Direction | Meaning |
| --- | --- | --- |
| 0 | input | Touch controller interrupt (active **low**) |
| 1 | output | Backlight rail enable |
| 5 | output | **LCD reset on rev2** (rev1 uses GPIO21 instead) |
| 6 | output | `SYS_EN` — battery power hold latch |
| 7 | output | Audio rail enable |

---

## 3. Rev1 vs Rev2: the trap that will cost you a day

**The backlight and touch-interrupt pins are swapped between revisions.**

| | rev1 | rev2 |
| --- | --- | --- |
| Backlight PWM | GPIO **8** | GPIO **42** |
| Touch IRQ | GPIO **42** | GPIO **8** |
| LCD reset | GPIO **21** | none — drive expander EXIO5 high |
| QSPI host | `SPI3_HOST` | `SPI2_HOST` (library default) |

A rev1 binary flashed to a rev2 board boots, mounts the SD card, talks to every I2C device, and
shows a black screen while the touch line PWMs at 25 kHz. Nothing errors. Get the revision right
before debugging anything else.

The clean way to handle this is a tiny per-revision header selected by a build flag, with everything
shared living in one common header:

```cpp
// rev2/Revision.h
constexpr int  kBacklightPin        = 42;
constexpr int  kDisplayGpioResetPin = -1;   // reset lives on expander EXIO5
constexpr int  kTouchIrqPin         = 8;

// rev1/Revision.h
constexpr int  kBacklightPin        = 8;
constexpr int  kDisplayGpioResetPin = 21;
constexpr int  kTouchIrqPin         = 42;
```

```ini
build_flags = -DBOARD_REVISION_HEADER=\"rev2/Revision.h\"
```

```cpp
#ifndef BOARD_REVISION_HEADER
#error "Board env must define BOARD_REVISION_HEADER."
#endif
#include BOARD_REVISION_HEADER
```

The `#error` is worth writing. A default-to-rev1 fallback is exactly the bug described above.

---

## 4. Bring-up order (this order matters)

```
1. pinMode boot/power/touch-IRQ pins (INPUT_PULLUP)
2. pinMode backlight OUTPUT, drive HIGH  ← HIGH == OFF, see §6
3. Wire.begin(17, 18)  @300 kHz, 10 ms timeout    (touch)
4. Wire1.begin(47, 48) @300 kHz, 10 ms timeout    (system)
5. TCA9554 EXIO6 = HIGH   ← battery power hold. Do this EARLY.
6. TCA9554 EXIO1 = HIGH   ← backlight rail
7. (rev2 only) TCA9554 EXIO5 = HIGH  ← release LCD reset
8. Panel init over QSPI, allocate framebuffer in PSRAM
9. Clear framebuffer, flush once, THEN enable backlight PWM
10. Touch probe, SD mount, audio/IMU as needed
```

Two ordering rules learned the hard way:

- **Enable the battery hold (EXIO6) before anything slow.** On battery power the board is held on by
  that latch. If your init sequence stalls before setting it — a blocking I2C probe, a slow SD
  mount — the board dies in your hand and it looks like a brownout.
- **Enable the backlight rail as part of display init, not before it.** Powering the panel rail
  while the controller is still un-initialized shows a bright frame of garbage on every boot. Init
  the panel, push a cleared frame, *then* turn on the light.

---

## 5. Display: QSPI + full framebuffer

### Bus setup

The panel is a 4-data-line QSPI device with no D/C pin; command/data framing is in the QSPI
instruction phase. With `Arduino_GFX`:

```cpp
Arduino_ESP32QSPI bus(/*cs*/9, /*sclk*/10, /*d0*/11, /*d1*/12, /*d2*/13, /*d3*/14, false);
Arduino_AXS15231B panel(&bus, /*rst*/ RESET_PIN_OR_MINUS_1, /*rotation*/0, /*ips*/false,
                        /*w*/172, /*h*/640, 0, 0, 0, 0);
panel.begin(40'000'000);
```

Findings:

- **40 MHz works** even though the library documents 32 MHz as the AXS15231B maximum and defaults to
  it. 40 MHz has been stable in shipping firmware on both revisions. Treat it as an empirical
  overclock: if you see tearing or corrupt rows on a new unit, drop back to 32 MHz first.
- **Raise `ESP32QSPI_MAX_PIXELS_AT_ONCE` to 2048** (library default 1024). This is the per-SPI-
  transaction pixel budget; 2048 pixels = 4 KB per transaction, which measurably cuts transaction
  overhead on a 220 KB frame.
- **Rev1 must use `SPI3_HOST`; rev2 uses the default `SPI2_HOST`.** Set with
  `-DESP32QSPI_SPI_HOST=SPI3_HOST` for rev1.
- If a new board shows nothing, `-DESP32QSPI_SPI_MODE=SPI_MODE3` is the second thing to try after
  reset wiring.

### Framebuffer strategy: draw to PSRAM, flush a prefix of rows

`172 × 640 × 2 bytes = 220,160 bytes` — too big for internal RAM, comfortable in PSRAM.

The pattern that works is a `Arduino_GFX`-derived canvas that:

1. allocates the framebuffer with `heap_caps_aligned_alloc(16, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)`,
2. renders every primitive into that buffer (no bus traffic per draw call),
3. tracks a single coalesced dirty rectangle,
4. on `flush()`, pushes **native rows `0..N`, full width**, where `N` is the bottom of the dirty
   region mapped back through the rotation.

The last point is the non-obvious one. Arbitrary `CASET`/`RASET` windows on this panel are fussy
enough (and rotation makes the mapping error-prone enough) that the reliable, still-fast compromise
is: never change the column window, only ever shorten the row count. You always send full-width
rows starting at row 0, so a change confined to the top of the screen costs a fraction of a frame,
while a change at the bottom costs a full frame. For a UI where the status bar and the active
content live near one edge, this wins most of the time with none of the windowing risk.

The dirty-row mapping per rotation, where the dirty rect is `(left, top, right, bottom)` in
logical coordinates and `HEIGHT` is the native panel height:

```cpp
switch (rotation) {
case 1:  return dirty_right;
case 2:  return HEIGHT - dirty_top;
case 3:  return HEIGHT - dirty_left;
default: return dirty_bottom;   // case 0
}
```

### One flush per frame

Every draw helper writes to PSRAM and marks dirty; **nothing** touches the bus until the frame ends:

```cpp
void endFrame() {
    if (anythingDrawn) gfx.flush();
}
```

A `pushColors()`-style blit must also *not* flush. Getting this wrong is the classic "why is my UI
at 4 fps" bug on this board: a per-widget flush turns one 220 KB transfer into fifty.

### Waking the panel

Redraw from the retained framebuffer rather than re-rendering the UI:

```cpp
void wake() {
    gfx.flush(/*force=*/true);   // repaint whole framebuffer
    setBacklightRail(true);
    setBacklight(true);
}
```

---

## 6. Backlight: inverted, with a large dead zone

Two independent controls:

1. **Rail enable** — TCA9554 EXIO1. Off here means genuinely no power to the LED string; use it for
   sleep.
2. **PWM brightness** — `analogWrite` on the backlight GPIO, 8-bit, **25 kHz**.

```cpp
analogWriteResolution(backlightPin, 8);
analogWriteFrequency(backlightPin, 25000);
analogWrite(backlightPin, 255 - duty);   // inverted: 0 = full on, 255 = off
```

Notes:

- **The PWM is inverted.** `0` is full brightness, `255` is off. To force the backlight off without
  killing the rail, `pinMode(OUTPUT); digitalWrite(HIGH);`.
- **There is a large dead zone at low duty.** Below roughly duty 100 the panel is simply dark, not
  dim. Map a user-facing 1–100 % scale onto duty `102..255` so "5 %" is a usable dim rather than a
  black screen:

```cpp
constexpr uint8_t kMinimumDuty = 102;
uint8_t duty = kMinimumDuty + (percent - 1) * (255 - kMinimumDuty) / 99;
```

- Use 25 kHz, not the Arduino default ~1 kHz — the panel's driver whines audibly in the low kHz
  range on some units.
- The inversion is documented in the source as a rev1 behavior but the same inverted path is used
  unconditionally for rev2 in shipping firmware, so treat "inverted on both" as the working
  assumption and re-verify on any new revision.

---

## 7. Touch: undocumented I2C command, hand-rolled decode

The AXS15231B touch interface is not a standard FT/CST-style register map. You write an 11-byte
command and read back an 8-byte packet.

```cpp
constexpr uint8_t kReadTouch[] = {
    0xB5, 0xAB, 0xA5, 0x5A, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00,
};

bool readPacket(TwoWire& wire, uint8_t addr, uint8_t* buf, size_t len /* == 8 */) {
    wire.beginTransmission(addr);
    wire.write(kReadTouch, sizeof(kReadTouch));
    if (wire.endTransmission(false) != 0) return false;      // repeated START, no STOP
    if (wire.requestFrom(addr, len, true) != len) return false;
    for (size_t i = 0; i < len; ++i) buf[i] = wire.read();
    return true;
}
```

Decode:

```cpp
// byte 0 must be zero. Idle frames come back nonzero-filled with a plausible
// point count in byte 1 — trusting byte 1 alone produces phantom touches.
if (data[0] != 0)                  { touched = false; return true; }
const uint8_t points = data[1];
if (points == 0 || points > 4)     { touched = false; return true; }

const uint16_t rawLong  = ((data[2] & 0x0F) << 8) | data[3];  // along the 640 axis
const uint16_t rawShort = ((data[4] & 0x0F) << 8) | data[5];  // along the 172 axis

x = min(rawShort, 172 - 1);
y = min(rawLong >= 640 ? 0 : 640 - 1 - rawLong, 640 - 1);      // long axis is inverted
```

Three details that are easy to get wrong:

- **Byte 0 is the real validity gate.** The controller returns idle frames that look like valid
  multi-touch data if you only check the point count.
- **The coordinate pair is (long, short), not (x, y).** The first pair is the 640-pixel axis.
- **The long axis is inverted** relative to the framebuffer, hence `640 - 1 - raw`.
- Only the low nibble of the high byte is coordinate data; mask with `0x0F`.

### Readiness: poll the expander, not the GPIO

The touch IRQ is available both as a direct GPIO and mirrored on TCA9554 **EXIO0** (active low). The
shipping firmware polls EXIO0 for "is there data" and reserves the direct GPIO for light-sleep wake.

```cpp
bool touchReady() {
    bool high = true;
    if (!tca9554ReadInputPin(Wire1, 0x20, /*EXIO*/0, high)) return false;
    return !high;      // active low
}
```

Timing that feels right: poll readiness every **5 ms**, and once a contact is active, read points
every **30 ms**. Polling readiness at the same slow rate as the points makes taps feel laggy;
reading points as fast as readiness saturates the I2C bus.

Also: **clear the expander interrupt before entering sleep.** A latched EXIO0 low means
`esp_light_sleep_start()` returns immediately, forever.

---

## 8. TCA9554 expander

Everything power-related routes through this chip, so its driver has to be boring and correct.

Registers: `0x00` input, `0x01` output, `0x03` config (1 = input, 0 = output).

Setting one output pin is a **read-modify-write of two registers**, in this order:

```cpp
bool configureOutputPin(TwoWire& w, uint8_t addr, uint8_t pin, bool high) {
    uint8_t output = 0xFF;
    if (!readReg(w, addr, 0x01, output)) return false;
    const uint8_t mask = 1u << pin;
    high ? (output |= mask) : (output &= ~mask);
    if (!writeReg(w, addr, 0x01, output)) return false;      // set level first

    uint8_t config = 0xFF;
    if (!readReg(w, addr, 0x03, config)) return false;
    config &= ~mask;                                          // then switch to output
    return writeReg(w, addr, 0x03, config);
}
```

Level before direction, always. Flipping direction first drives the pin to whatever was latched,
which on EXIO6 means momentarily cutting your own power.

On this board a plain `endTransmission(true)` followed by `requestFrom` works; there is no need for
the bus-release-and-delay dance some expander drivers use. Keep it as a flag if you plan to share
the driver across boards.

---

## 9. Power

### The battery hold latch — the single most important line of code

```cpp
tca9554ConfigureOutputPin(Wire1, 0x20, /*EXIO6 SYS_EN*/6, true);   // stay alive
```

On USB the board runs regardless. **On battery it stays on only while EXIO6 is high.** Set it as
early in `setup()` as you can get I2C1 up. Clearing it is your clean power-off:

```cpp
bool powerOff() {
    return tca9554ConfigureOutputPin(Wire1, 0x20, 6, false);
}
```

### Battery voltage

GPIO4, ADC1, 12-bit, `ADC_11db`, hardware divider ratio **3.0**.

The raw ADC on an ESP32-S3 is noisy enough that a naive single read swings ±100 mV, which reads as a
battery bouncing between 60 % and 80 %. What works:

1. `delay(12)` to let the divider settle after enabling the pin.
2. Take 24 samples via `analogReadMilliVolts()` (uses the factory eFuse calibration), **discarding
   the first 2**.
3. Sort, trim 2 from each end, average the rest.
4. `voltage = mean_mV * 3.0 / 1000.0`
5. Sanity gate: treat `2.5 V .. 4.6 V` as "battery present", anything else as absent.
6. Fall back to raw `analogRead()` × `3300/4095` only if `analogReadMilliVolts` returns zeros.

A Li-ion percentage curve that tracks this cell reasonably:

| V | % | V | % | V | % |
| --- | --- | --- | --- | --- | --- |
| 3.30 | 0 | 3.70 | 30 | 3.92 | 70 |
| 3.50 | 5 | 3.75 | 40 | 4.00 | 80 |
| 3.60 | 10 | 3.79 | 50 | 4.10 | 90 |
| 3.65 | 20 | 3.85 | 60 | 4.15 | 100 |

Interpolate linearly between points and clamp at both ends. Note how compressed 3.65–3.85 V is —
that flat region is most of the battery's life, and a linear voltage-to-percent map makes the gauge
appear stuck at 50 % for hours and then plummet.

**There is no PMIC.** Unlike Waveshare's AMOLED boards (AXP2101), this board exposes no charge
status, no VBUS-present sense, and no power-button-held register. If your UI wants "charging" or
"on USB" state, you have to infer it or do without.

---

## 10. Storage: SD over SDMMC, 1-bit

Only CLK/CMD/D0 are wired, so 1-bit mode is mandatory:

```cpp
SD_MMC.setPins(/*clk*/GPIO_NUM_41, /*cmd*/GPIO_NUM_39, /*d0*/GPIO_NUM_40);
SD_MMC.begin(mountPoint, /*mode1bit=*/true, /*format_if_empty=*/false, freqKhz, maxOpenFiles);
```

**Cards vary wildly, and a card that mounts is not a card that works.** The reliable strategy is a
frequency ladder with a *sustained read-write probe* at each step, not just a successful mount:

```
SDMMC_FREQ_HIGHSPEED (40 MHz)
SDMMC_FREQ_DEFAULT   (20 MHz)
10000 kHz             ← some cards are unstable at the Arduino default
SDMMC_FREQ_PROBING   (400 kHz)
```

At each candidate: unmount, mount at that frequency, then write and read back ~256 KB in 4 KB chunks
verifying a generated pattern. Only a frequency that survives the probe is accepted. Cache the
winning frequency (with a card-identity check, so swapping cards re-probes) in NVS so subsequent
boots skip the ladder.

The failure this catches: a marginal card mounts fine at 40 MHz and returns corrupt data a few
hundred KB into a large file. A mount-only check will not see it.

---

## 11. USB mass storage — the build-flag conflict

Exposing the SD card as a USB drive requires TinyUSB, which means:

```ini
build_flags  = -DARDUINO_USB_MODE=0 -DARDUINO_USB_MSC_ON_BOOT=0
build_unflags = -DARDUINO_USB_MODE=1
```

The board definition ships `-DARDUINO_USB_MODE=1` (hardware CDC). You **must unflag it** — adding
`-DARDUINO_USB_MODE=0` while `=1` is still on the command line is a macro redefinition, not a
silent override, and which value wins is not something to rely on.

Consequences to plan for:

- Switching to TinyUSB mode changes serial-monitor behavior. Keep `ARDUINO_USB_CDC_ON_BOOT=1`.
- `ARDUINO_USB_MSC_ON_BOOT=0` — mount the MSC device on demand, not at boot, or you can never write
  to the card from firmware.
- Take the SD card away from `SD_MMC` and hand it to the raw `sdmmc_host` driver while MSC is
  active; the two cannot share the card. Deinit with `sdmmc_host_deinit()` and tolerate
  `ESP_ERR_INVALID_STATE`.
- After `msc.begin(blockCount, 512)`, force host re-enumeration or the drive often does not appear:

```cpp
tud_disconnect(); delay(120); tud_connect();
```

- Run the same frequency ladder for the MSC path; a card that needed 10 MHz for the filesystem needs
  it for block access too.

---

## 12. Audio and IMU

**ES8311** at `0x18` on I2C1, I2S0 for data. The audio rail must be enabled first:

```cpp
tca9554ConfigureOutputPin(Wire1, 0x20, /*EXIO7*/7, true);
```

Wiring is MCLK 7, BCLK 15, WS 46, plus the two data nets DOUT 45 and DIN 6. Playback in the
reference firmware drives **GPIO45** as the I2S data-out line; confirm the direction of GPIO6
against the schematic before wiring up capture. For playback the codec needs MCLK — it is not
optional on this part.

**QMI8658** at `0x6B` on I2C1. Plain register read/write; nothing board-specific. Useful for
auto-rotation, since the 640 × 172 panel is used in both landscape orientations.

---

## 13. Sleep and wake

Light sleep is the right primitive here — deep sleep loses the PSRAM framebuffer and the wake feels
like a reboot.

```cpp
EspLightSleep::WakeReason wait(std::span<const gpio_num_t> wakePins, uint32_t timeoutMs) {
    // 1. Wait for every wake pin to be released, or you sleep and wake instantly.
    for (auto pin : wakePins) pinMode(pin, INPUT_PULLUP);
    while (!all_of(wakePins, [](auto p){ return digitalRead(p); })) delay(10);

    // 2. Arm level-triggered GPIO wake.
    for (auto pin : wakePins) gpio_wakeup_enable(pin, GPIO_INTR_LOW_LEVEL);
    esp_sleep_enable_gpio_wakeup();
    if (timeoutMs) esp_sleep_enable_timer_wakeup(uint64_t(timeoutMs) * 1000ULL);

    esp_light_sleep_start();
    auto cause = esp_sleep_get_wakeup_cause();

    // 3. ALWAYS disarm, on every exit path.
    for (auto pin : wakePins) gpio_wakeup_disable(pin);
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);
    if (timeoutMs) esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
    ...
}
```

Board specifics:

- Wake pins: **GPIO16 (PWR button)** and the revision's **touch IRQ pin** (rev2: 8, rev1: 42).
- **Clear the TCA9554 EXIO0 latch before sleeping** by reading the input register. A pending touch
  interrupt makes light sleep a no-op.
- **Drain the release wait first.** Calling sleep while the user's finger is still on the button is
  the number-one cause of "sleep does nothing."
- Disarm the wake sources after waking. Leaving GPIO wake armed corrupts the *next* sleep's wake
  cause, which presents as random spurious wakes.
- On sleep: backlight PWM off, then TCA9554 EXIO1 (rail) off. On wake: repaint the framebuffer,
  rail on, PWM on — in that order, or you flash the previous screen contents.

---

## 14. Working PlatformIO configuration

```ini
[env:waveshare_esp32s3_touch_lcd_349_rev2]
platform = https://github.com/pioarduino/platform-espressif32/releases/download/55.03.39/platform-espressif32.zip
board = esp32-s3-r8-opi          ; custom: 16MB QIO flash + 8MB OPI PSRAM
framework = arduino
platform_packages =
  framework-arduinoespressif32 @ https://github.com/espressif/arduino-esp32/releases/download/3.3.9/esp32-core-3.3.9.tar.xz
  framework-arduinoespressif32-libs @ https://github.com/espressif/arduino-esp32/releases/download/3.3.9/esp32-core-3.3.9-libs.tar.xz

monitor_speed = 115200
monitor_filters = default, time, esp32_exception_decoder
upload_speed = 460800

lib_deps = https://github.com/moononournation/Arduino_GFX.git

build_flags =
  -DCORE_DEBUG_LEVEL=3
  -DLOG_LOCAL_LEVEL=CORE_DEBUG_LEVEL
  -DESP32QSPI_MAX_PIXELS_AT_ONCE=2048
  ; rev1 only:  -DESP32QSPI_SPI_HOST=SPI3_HOST
  ; if blank screen persists: -DESP32QSPI_SPI_MODE=SPI_MODE3
  -DARDUINO_USB_MODE=0            ; TinyUSB, only if you need USB MSC
  -DARDUINO_USB_MSC_ON_BOOT=0
build_unflags =
  -DARDUINO_USB_MODE=1
```

A custom board JSON (`boards/esp32-s3-r8-opi.json`) pins down the 16 MB flash + 8 MB OPI PSRAM
combination rather than relying on a stock devkit definition matching it:

```json
{
  "build": {
    "arduino": { "ldscript": "esp32s3_out.ld",
                 "memory_type": "qio_opi",
                 "partitions": "default_16MB.csv" },
    "core": "esp32",
    "extra_flags": ["-DARDUINO_ESP32S3_DEV_16M_OPI", "-DARDUINO_USB_MODE=1",
                    "-DARDUINO_USB_CDC_ON_BOOT=1", "-DARDUINO_RUNNING_CORE=1",
                    "-DARDUINO_EVENT_RUNNING_CORE=1", "-DBOARD_HAS_PSRAM"],
    "f_cpu": "240000000L", "f_flash": "80000000L", "flash_mode": "qio",
    "hwids": [["0x303A", "0x1001"]],
    "mcu": "esp32s3", "variant": "esp32s3"
  },
  "frameworks": ["arduino", "espidf"],
  "name": "ESP32-S3 Dev Module (16M Flash 8M OPI PSRAM)",
  "upload": { "flash_size": "16MB", "maximum_ram_size": 327680,
              "maximum_size": 16777216, "require_upload_port": true, "speed": 460800 }
}
```

`"memory_type": "qio_opi"` is the load-bearing field. Without it `psramFound()` returns false and
your framebuffer allocation silently falls back to internal RAM and fails.

Build and flash:

```bash
pio run -e waveshare_esp32s3_touch_lcd_349_rev2 -t upload
pio device monitor
```

---

## 15. An architecture worth copying

If you plan more than one app or more than one board, the split that held up well:

```
board/       # app-facing API only: Display, Input, Power, Storage, Audio, Imu, System
             # headers with no implementation, no chip names, no pin numbers
platforms/   # one folder per board family. Owns pins, rails, reset order, bus choice.
             # A private Board.h of constexpr namespaces: DisplayWiring, TouchWiring,
             # Power, Storage, System, Tca9554Wiring, AudioWiring, Buttons.
drivers/     # reusable chip code: tca9554/, axs15231b_touch/, es8311/, qmi8658/
             # validates chip facts (addresses, packet lengths, masks). Knows no board.
app/         # everything else. Never sees a GPIO number.
```

Rules that made it work:

- **Select the platform folder in the build, not with `#ifdef`.** The base `build_src_filter`
  excludes all of `platforms/**` and `drivers/**`; each environment adds back exactly one platform
  folder and only the drivers that board uses. Unused code never compiles, so it never silently rots.
- **Board config is public facts only** — board id, OTA asset name, display dimensions, default
  orientation, UI margins. A TCA9554 address or backlight GPIO in the public config is a design
  smell; it belongs in the platform's private header.
- **Raw input, not logical input, crosses the boundary.** The platform reports "BOOT is down" and
  "contact at (x, y)". Debounce, long-press, triple-press, swipe and edge-gesture logic lives once
  in shared code. Otherwise every new board reimplements gesture handling slightly differently.
- **Revisions are headers, not `#ifdef`s.** One platform folder, one `Revision.h` per revision,
  selected by a `-D` path. The rev1/rev2 pin swap in §3 stays a two-line diff.

---

## 16. Gotcha checklist

- [ ] Rev1 and rev2 **swap the backlight and touch-IRQ pins**. Verify your board revision first.
- [ ] Rev2 has **no GPIO LCD reset** — it is TCA9554 EXIO5.
- [ ] Rev1 needs **`SPI3_HOST`**; rev2 uses the default SPI2.
- [ ] Set **TCA9554 EXIO6 (SYS_EN) high early** or the board powers off on battery.
- [ ] Backlight PWM is **inverted** (0 = bright) and **dark below ~duty 100**.
- [ ] Enable the backlight rail **after** panel init, or every boot flashes garbage.
- [ ] Touch packet **byte 0 must be zero**; the point count alone yields phantom touches.
- [ ] Touch coordinates arrive as **(long axis, short axis)** and the long axis is **inverted**.
- [ ] **Flush once per frame.** Per-widget flushes cost you an order of magnitude.
- [ ] `"memory_type": "qio_opi"` in the board JSON, or there is no PSRAM.
- [ ] Do not use **GPIO 26–37** — octal flash/PSRAM.
- [ ] SD is **1-bit only** (no D1–D3) and needs a **frequency ladder with a sustained data probe**.
- [ ] USB MSC needs `ARDUINO_USB_MODE=0` **and** `build_unflags = -DARDUINO_USB_MODE=1`.
- [ ] **Clear the EXIO0 touch-interrupt latch and wait for button release** before light sleep.
- [ ] **Disarm GPIO/timer wake sources** after every light sleep.
- [ ] Set an explicit **`Wire.setTimeOut(10)`** on both buses.
- [ ] There is **no PMIC**: no charge status, no VBUS sense, no power-button register.

---

## 17. Reference links

- Product page: <https://www.waveshare.com/esp32-s3-touch-lcd-3.49.htm>
- Wiki: <https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-3.49>
- `Arduino_GFX` (has the `Arduino_ESP32QSPI` bus and `Arduino_AXS15231B` panel driver):
  <https://github.com/moononournation/Arduino_GFX>
- pioarduino ESP32 platform (needed for recent Arduino cores under PlatformIO):
  <https://github.com/pioarduino/platform-espressif32>
