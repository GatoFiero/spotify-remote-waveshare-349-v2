#pragma once
// The generated fonts cover Latin-1, which handles most of what Spotify returns.
// Anything outside it gets folded down rather than dropped.

#include <stddef.h>
#include <stdint.h>

namespace text {

// Converts UTF-8 to Latin-1 in place of a copy, mapping the typographic
// punctuation Spotify metadata is full of (curly quotes, en dashes, ellipses)
// onto ASCII equivalents. Unrepresentable codepoints become '?'.
void toLatin1(const char *utf8, char *out, size_t out_size);

// "3:07" / "1:04:22"
void formatDuration(uint32_t milliseconds, char *out, size_t out_size);

}  // namespace text
