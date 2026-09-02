#pragma once
// Downloads and decodes the album cover into a PSRAM RGB565 buffer.

#include <WiFiClientSecure.h>
#include <stdint.h>

namespace albumart {

constexpr int16_t kSize = 150;  // Spotify's 300 px image at half scale lands exactly here

bool begin();

// Fetches and decodes `url`. Blocking; call from the network task only.
// Leaves the previous image intact on failure.
bool load(const char *url, uint32_t generation);

// Null until at least one cover has decoded. kSize x kSize, RGB565 host order.
const uint16_t *pixels();

// Generation of the track the current image belongs to; 0 if none.
uint32_t generation();

// Drops the current image (used when playback stops).
void clear();

}  // namespace albumart
