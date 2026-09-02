#include "Axs15231bTouch.h"

#include <string.h>

#include <algorithm>

namespace axs15231b {
namespace {

constexpr uint8_t kReadTouchCommand[] = {
    0xB5, 0xAB, 0xA5, 0x5A, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00,
};
constexpr size_t kPacketLength = 8;

bool readPacket(TwoWire &wire, uint8_t address, uint8_t *buffer) {
    wire.beginTransmission(address);
    wire.write(kReadTouchCommand, sizeof(kReadTouchCommand));
    if (wire.endTransmission(false) != 0) return false;  // repeated START, no STOP
    if (wire.requestFrom(address, static_cast<uint8_t>(kPacketLength)) != kPacketLength) {
        return false;
    }
    for (size_t i = 0; i < kPacketLength; ++i) buffer[i] = wire.read();
    return true;
}

}  // namespace

#ifdef TOUCH_DEBUG
uint8_t last_packet[8] = {};
#endif

bool readTouch(TwoWire &wire, uint8_t address, int16_t panel_width, int16_t panel_height,
               bool &touched, Point &point) {
    uint8_t data[kPacketLength] = {};
    if (!readPacket(wire, address, data)) return false;
#ifdef TOUCH_DEBUG
    memcpy(last_packet, data, kPacketLength);
#endif

    // Byte 0 is the real validity gate. Idle frames come back nonzero-filled with a
    // plausible point count in byte 1; trusting byte 1 alone produces phantom touches.
    if (data[0] != 0) {
        touched = false;
        return true;
    }
    const uint8_t points = data[1];
    if (points == 0 || points > 4) {
        touched = false;
        return true;
    }

    // The coordinate pair arrives as (long axis, short axis), and only the low
    // nibble of each high byte is coordinate data.
    const uint16_t raw_long  = ((data[2] & 0x0F) << 8) | data[3];
    const uint16_t raw_short = ((data[4] & 0x0F) << 8) | data[5];

    point.native_x = std::min<uint16_t>(raw_short, panel_width - 1);
    point.native_y = std::min<uint16_t>(raw_long >= panel_height ? 0 : panel_height - 1 - raw_long,
                                   panel_height - 1);  // the long axis is inverted
    touched = true;
    return true;
}

}  // namespace axs15231b
