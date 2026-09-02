#include "Text.h"

#include <stdint.h>
#include <stdio.h>

namespace text {
namespace {

// Returns the ASCII (or Latin-1) stand-in for a codepoint, or 0 if there is none.
char fold(uint32_t cp) {
    switch (cp) {
        case 0x2018: case 0x2019: case 0x02BC: return '\'';
        case 0x201C: case 0x201D: return '"';
        case 0x2010: case 0x2011: case 0x2012:
        case 0x2013: case 0x2014: case 0x2015: return '-';
        case 0x00A0: case 0x2007: case 0x202F: return ' ';
        case 0x2022: return '*';
        default: return 0;
    }
}

}  // namespace

void toLatin1(const char *utf8, char *out, size_t out_size) {
    if (out_size == 0) return;
    size_t written = 0;
    const uint8_t *p = reinterpret_cast<const uint8_t *>(utf8);

    while (*p && written + 1 < out_size) {
        uint32_t cp;
        int extra;
        if (*p < 0x80) {
            cp = *p++;
            extra = 0;
        } else if ((*p & 0xE0) == 0xC0) {
            cp = *p++ & 0x1F;
            extra = 1;
        } else if ((*p & 0xF0) == 0xE0) {
            cp = *p++ & 0x0F;
            extra = 2;
        } else if ((*p & 0xF8) == 0xF0) {
            cp = *p++ & 0x07;
            extra = 3;
        } else {
            ++p;  // stray continuation byte
            continue;
        }
        for (int i = 0; i < extra; ++i) {
            if ((*p & 0xC0) != 0x80) break;  // truncated sequence
            cp = (cp << 6) | (*p++ & 0x3F);
        }

        if (cp == 0x2026) {  // horizontal ellipsis needs three characters
            for (int i = 0; i < 3 && written + 1 < out_size; ++i) out[written++] = '.';
            continue;
        }
        if (cp < 0x100) {
            out[written++] = static_cast<char>(cp);
        } else if (const char folded = fold(cp)) {
            out[written++] = folded;
        } else {
            out[written++] = '?';
        }
    }
    out[written] = '\0';
}

void formatDuration(uint32_t milliseconds, char *out, size_t out_size) {
    const uint32_t total_seconds = milliseconds / 1000;
    const uint32_t hours = total_seconds / 3600;
    const uint32_t minutes = (total_seconds / 60) % 60;
    const uint32_t seconds = total_seconds % 60;
    if (hours > 0) {
        snprintf(out, out_size, "%lu:%02lu:%02lu", hours, minutes, seconds);
    } else {
        snprintf(out, out_size, "%lu:%02lu", minutes, seconds);
    }
}

}  // namespace text
