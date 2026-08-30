#include "Touch.h"

#include <Wire.h>

#include "../drivers/Axs15231bTouch.h"
#include "Board.h"
#include "Hardware.h"
#include "Screen.h"

namespace touch {
namespace {

constexpr uint32_t kReadyPollMs = 5;
constexpr uint32_t kPointPollMs = 30;
constexpr uint32_t kReleaseGraceMs = 60;  // ride out the gaps between contact reports

Contact contact;
uint32_t last_ready_poll_ms = 0;
uint32_t last_point_poll_ms = 0;
uint32_t last_contact_ms = 0;

// The canvas lays out the framebuffer so that (rotation 1) native row == logical
// x and native column == 171 - logical y; rotation 3 is the 180-degree flip of
// that. Touch reports (short axis, long axis) in that same native frame.
void toLogical(const axs15231b::Point &p, int16_t &x, int16_t &y) {
#if UI_ROTATION == 1
    x = p.native_y;
    y = board::display_wiring::kPanelWidth - 1 - p.native_x;
#else
    x = board::display_wiring::kPanelHeight - 1 - p.native_y;
    y = p.native_x;
#endif

#ifdef TOUCH_FLIP_X
    x = screen::kWidth - 1 - x;
#endif
#ifdef TOUCH_FLIP_Y
    y = screen::kHeight - 1 - y;
#endif

    x = constrain(x, 0, screen::kWidth - 1);
    y = constrain(y, 0, screen::kHeight - 1);
}

}  // namespace

bool begin() {
    Wire.beginTransmission(board::touch_wiring::kI2cAddress);
    return Wire.endTransmission(true) == 0;
}

void poll() {
    const uint32_t now = millis();
    if (now - last_ready_poll_ms < kReadyPollMs) return;
    last_ready_poll_ms = now;

    const bool ready = hardware::touchDataReady();
    if (!ready && !contact.down) return;

    if (now - last_point_poll_ms < kPointPollMs) return;
    last_point_poll_ms = now;

    bool touched = false;
    axs15231b::Point point;
    const bool ok = axs15231b::readTouch(Wire, board::touch_wiring::kI2cAddress,
                                         board::display_wiring::kPanelWidth,
                                         board::display_wiring::kPanelHeight, touched, point);

    if (ok && touched) {
        toLogical(point, contact.x, contact.y);
        contact.down = true;
        last_contact_ms = now;
#ifdef TOUCH_DEBUG
        log_i("touch raw=[%02X %02X %02X %02X %02X %02X] native=(%u,%u) logical=(%d,%d)",
              axs15231b::last_packet[0], axs15231b::last_packet[1], axs15231b::last_packet[2],
              axs15231b::last_packet[3], axs15231b::last_packet[4], axs15231b::last_packet[5],
              point.native_x, point.native_y, contact.x, contact.y);
#endif
    } else if (contact.down && now - last_contact_ms > kReleaseGraceMs) {
        contact.down = false;
    }
}

const Contact &current() { return contact; }

}  // namespace touch
