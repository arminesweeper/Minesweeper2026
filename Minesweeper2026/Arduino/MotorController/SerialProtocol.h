/**
 * ============================================================================
 * SERIALPROTOCOL.H - Serial Communication Protocol
 * ============================================================================
 */

#ifndef SERIALPROTOCOL_H
#define SERIALPROTOCOL_H

#include "Config.h"
#include <Arduino.h>


/**
 * @brief Parsed command structure
 */
struct ParsedCommand {
  char wheel;      // 'r' for right, 'l' for left
  char sign;       // 'p' for positive, 'n' for negative
  double velocity; // Velocity magnitude in rad/s
  bool valid;      // True if command was successfully parsed
};

/**
 * @brief Serial Protocol Handler
 *
 * Handles bidirectional serial communication:
 * - Incoming: "r[p|n]XX.XX,l[p|n]XX.XX,\n"
 * - Outgoing: "r[p|n]X.XXX,l[p|n]X.XXX,\n"
 *
 * Features:
 * - Fixed-size buffer (no dynamic allocation)
 * - State machine parser
 * - Overflow protection
 * - Packet framing
 */
class SerialProtocol {
public:
  /**
   * @brief Parser state machine states
   */
  enum class State : uint8_t {
    WAITING_PREFIX,
    READING_DIRECTION,
    READING_VALUE,
    COMPLETE
  };

  /**
   * @brief Construct a new Serial Protocol handler
   */
  SerialProtocol();

  /**
   * @brief Initialize serial communication
   */
  void begin();

  /**
   * @brief Process incoming serial data
   * @param right_vel Output: right wheel command velocity
   * @param left_vel Output: left wheel command velocity
   * @return true if a complete command packet was received
   */
  bool processInput(double &right_vel, double &left_vel);

  /**
   * @brief Send telemetry data
   * @param right_vel Right wheel measured velocity (rad/s)
   * @param left_vel Left wheel measured velocity (rad/s)
   */
  void sendTelemetry(double right_vel, double left_vel) const;

  /**
   * @brief Send odometry data
   * @param x X position in meters
   * @param y Y position in meters
   * @param theta Heading in radians
   */
  void sendOdometry(double x, double y, double theta) const;

  /**
   * @brief Send status message
   * @param message Null-terminated string message
   */
  void sendStatus(const char *message) const;

  /**
   * @brief Send error message
   * @param error_code Error identifier
   * @param message Description
   */
  void sendError(uint8_t error_code, const char *message) const;

  /**
   * @brief Get time of last received command
   * @return millis() timestamp of last complete command
   */
  unsigned long getLastCommandTime() const { return last_command_time_; }

  /**
   * @brief Check if data is available
   * @return true if there's data in the receive buffer
   */
  bool dataAvailable() const { return Serial.available() > 0; }

private:
  State parser_state_;
  char rx_buffer_[SerialConfig::RX_BUFFER_SIZE];
  size_t rx_index_;
  unsigned long last_command_time_;

  // Temporary parsing storage
  bool parsing_right_;
  char current_sign_;
  char value_buffer_[8];
  size_t value_index_;

  // Pending command values
  double pending_right_vel_;
  double pending_left_vel_;
  bool right_received_;
  bool left_received_;

  void resetParser();
  void resetValueBuffer();
  ParsedCommand parseSingleCommand(const char *wheel_char,
                                   const char *sign_char,
                                   const char *value_str);
  void processCompletePacket();
};

#endif // SERIALPROTOCOL_H