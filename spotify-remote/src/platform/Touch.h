#pragma once
// Raw contact reporting in logical (640x172) screen coordinates. Tap and
// long-press logic lives above this, in the app.

#include <stdint.h>

namespace touch {

struct Contact {
    bool down = false;
    int16_t x = 0;
    int16_t y = 0;
};

bool begin();

// Polls readiness every ~5 ms and coordinates every ~30 ms. Polling readiness as
// slowly as the coordinates makes taps feel laggy; reading coordinates as fast
// as readiness saturates the I2C bus.
void poll();

const Contact &current();

}  // namespace touch
