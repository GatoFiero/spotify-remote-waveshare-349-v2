#pragma once
// AXS15231B touch interface. Not a standard FT/CST register map: you write an
// 11-byte command and read back an 8-byte packet.

#include <Arduino.h>
#include <Wire.h>
#include <stdint.h>

namespace axs15231b {

struct Point {
    uint16_t native_x = 0;  // 0..171, across the short axis
    uint16_t native_y = 0;  // 0..639, along the long axis
};

// Reads one packet. Returns false on a bus error. `touched` is false for a valid
// read that reports no contact.
bool readTouch(TwoWire &wire, uint8_t address, int16_t panel_width, int16_t panel_height,
               bool &touched, Point &point);

#ifdef TOUCH_DEBUG
// Raw packet bytes from the last readTouch(), for calibration.
extern uint8_t last_packet[8];
#endif

}  // namespace axs15231b
