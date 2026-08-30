#pragma once
// Minimal TCA9554 GPIO expander driver. Knows nothing about which board it is on.

#include <Wire.h>
#include <stdint.h>

namespace tca9554 {

// Registers: 0x00 input, 0x01 output, 0x03 configuration (1 = input, 0 = output).
bool readRegister(TwoWire &wire, uint8_t address, uint8_t reg, uint8_t &value);
bool writeRegister(TwoWire &wire, uint8_t address, uint8_t reg, uint8_t value);

// Drives one pin as an output at the given level. Sets the level *before*
// switching direction -- the other order momentarily drives the pin to whatever
// was latched, which on the SYS_EN pin means cutting your own power.
bool configureOutputPin(TwoWire &wire, uint8_t address, uint8_t pin, bool high);

// Reads one pin from the input register. Also clears a latched interrupt, which
// is why it must be called before entering light sleep.
bool readInputPin(TwoWire &wire, uint8_t address, uint8_t pin, bool &high);

}  // namespace tca9554
