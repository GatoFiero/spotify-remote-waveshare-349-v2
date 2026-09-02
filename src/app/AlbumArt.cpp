#include "AlbumArt.h"

#include <HTTPClient.h>
#include <JPEGDEC.h>
#include <esp_heap_caps.h>

#include "Net.h"

namespace albumart {
namespace {

// Spotify's 300x300 covers are 15-30 KB; leave generous headroom for the 640 px
// variant in case a release has no mid-size image.
constexpr size_t kMaxJpegBytes = 192 * 1024;

uint16_t *rgb565 = nullptr;
uint8_t *jpeg_bytes = nullptr;
uint32_t current_generation = 0;

JPEGDEC decoder;
int16_t draw_offset_x = 0;
int16_t draw_offset_y = 0;

// JPEGDEC hands back MCU blocks; place them into the square buffer, clipping
// anything that falls outside (covers are square, but do not assume it).
int drawBlock(JPEGDRAW *block) {
    for (int row = 0; row < block->iHeight; ++row) {
        const int y = block->y + row + draw_offset_y;
        if (y < 0 || y >= kSize) continue;
        const uint16_t *src = block->pPixels + row * block->iWidth;
        for (int col = 0; col < block->iWidthUsed; ++col) {
            const int x = block->x + col + draw_offset_x;
            if (x < 0 || x >= kSize) continue;
            rgb565[y * kSize + x] = src[col];
        }
    }
    return 1;
}

}  // namespace

bool begin() {
    rgb565 = static_cast<uint16_t *>(
        heap_caps_aligned_alloc(16, kSize * kSize * sizeof(uint16_t), MALLOC_CAP_SPIRAM));
    jpeg_bytes = static_cast<uint8_t *>(heap_caps_malloc(kMaxJpegBytes, MALLOC_CAP_SPIRAM));
    if (!rgb565 || !jpeg_bytes) {
        log_e("no PSRAM for album art buffers");
        return false;
    }
    memset(rgb565, 0, kSize * kSize * sizeof(uint16_t));
    return true;
}

bool load(const char *url, uint32_t generation) {
    if (!rgb565 || !jpeg_bytes || !url || !*url) return false;

    size_t length = 0;
    {
        WiFiClientSecure client;
        net::secure(client);
        HTTPClient http;
        http.setTimeout(10000);
        if (!http.begin(client, url)) return false;

        const int code = http.GET();
        if (code != HTTP_CODE_OK) {
            log_w("album art HTTP %d", code);
            http.end();
            return false;
        }

        const int content_length = http.getSize();
        if (content_length > 0 && static_cast<size_t>(content_length) > kMaxJpegBytes) {
            log_w("album art too large: %d bytes", content_length);
            http.end();
            return false;
        }

        auto *stream = http.getStreamPtr();
        const uint32_t deadline = millis() + 15000;
        while (http.connected() && length < kMaxJpegBytes && millis() < deadline) {
            const size_t available = stream->available();
            if (available == 0) {
                if (content_length > 0 && length >= static_cast<size_t>(content_length)) break;
                delay(5);
                continue;
            }
            const int read = stream->readBytes(jpeg_bytes + length,
                                               min(available, kMaxJpegBytes - length));
            if (read <= 0) break;
            length += read;
            if (content_length > 0 && length >= static_cast<size_t>(content_length)) break;
        }
        http.end();
    }

    if (length < 128) {
        log_w("album art download produced only %u bytes", length);
        return false;
    }

    if (!decoder.openRAM(jpeg_bytes, length, drawBlock)) {
        log_w("album art JPEG rejected");
        return false;
    }
    decoder.setPixelType(RGB565_LITTLE_ENDIAN);

    // Pick the largest scale that still covers our square. Spotify's 300 px
    // cover at half scale lands on exactly 150.
    const int source_w = decoder.getWidth();
    const int source_h = decoder.getHeight();
    int options = 0, divisor = 1;
    if (source_w >= kSize * 8)      { options = JPEG_SCALE_EIGHTH;  divisor = 8; }
    else if (source_w >= kSize * 4) { options = JPEG_SCALE_QUARTER; divisor = 4; }
    else if (source_w >= kSize * 2) { options = JPEG_SCALE_HALF;    divisor = 2; }

    draw_offset_x = (kSize - source_w / divisor) / 2;  // centre, cropping any overflow
    draw_offset_y = (kSize - source_h / divisor) / 2;

    // current_generation stays 0 across the memset and decode, so the UI task
    // draws the placeholder rather than a half-written buffer.
    current_generation = 0;
    memset(rgb565, 0, kSize * kSize * sizeof(uint16_t));
    const int ok = decoder.decode(0, 0, options);
    decoder.close();

    if (!ok) {
        log_w("album art decode failed");
        return false;
    }
    current_generation = generation;
    return true;
}

const uint16_t *pixels() { return current_generation ? rgb565 : nullptr; }
uint32_t generation() { return current_generation; }
void clear() { current_generation = 0; }

}  // namespace albumart
