#include "Tca9554.h"

namespace tca9554 {
namespace {
constexpr uint8_t kInputRegister  = 0x00;
constexpr uint8_t kOutputRegister = 0x01;
constexpr uint8_t kConfigRegister = 0x03;
}  // namespace

bool readRegister(TwoWire &wire, uint8_t address, uint8_t reg, uint8_t &value) {
    wire.beginTransmission(address);
    wire.write(reg);
    if (wire.endTransmission(true) != 0) return false;
    if (wire.requestFrom(address, static_cast<uint8_t>(1)) != 1) return false;
    value = wire.read();
    return true;
}

bool writeRegister(TwoWire &wire, uint8_t address, uint8_t reg, uint8_t value) {
    wire.beginTransmission(address);
    wire.write(reg);
    wire.write(value);
    return wire.endTransmission(true) == 0;
}

bool configureOutputPin(TwoWire &wire, uint8_t address, uint8_t pin, bool high) {
    const uint8_t mask = 1u << pin;

    uint8_t output = 0xFF;
    if (!readRegister(wire, address, kOutputRegister, output)) return false;
    high ? (output |= mask) : (output &= ~mask);
    if (!writeRegister(wire, address, kOutputRegister, output)) return false;  // level first

    uint8_t config = 0xFF;
    if (!readRegister(wire, address, kConfigRegister, config)) return false;
    config &= ~mask;                                                          // then direction
    return writeRegister(wire, address, kConfigRegister, config);
}

bool readInputPin(TwoWire &wire, uint8_t address, uint8_t pin, bool &high) {
    uint8_t input = 0;
    if (!readRegister(wire, address, kInputRegister, input)) return false;
    high = (input & (1u << pin)) != 0;
    return true;
}

}  // namespace tca9554
