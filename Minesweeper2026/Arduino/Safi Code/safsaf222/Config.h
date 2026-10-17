/**
 * Minimal serial-protocol config for safsaf222.
 * Matches MotorController SerialConfig / ENABLE_SERIAL2 so SerialProtocol.cpp
 * can be shared with the RPi / control-station protocol.
 */
#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

#ifndef ENABLE_SERIAL2
#define ENABLE_SERIAL2 0
#endif

namespace SerialConfig {
constexpr unsigned long BAUD_RATE = 115200;
constexpr size_t RX_BUFFER_SIZE = 32;
constexpr size_t TX_BUFFER_SIZE = 64;
constexpr size_t CMD_BUFFER_SIZE = 24;
constexpr size_t TEXT_BUFFER_SIZE = 64;
}  // namespace SerialConfig

#endif
