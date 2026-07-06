/**
 * ============================================================================
 * SERIALPROTOCOL.H - Serial Communication Protocol
 * ============================================================================
 * Handles bidirectional serial communication:
 * - Incoming: Velocity commands ("rp02.50,ln01.30,") and Extended ("C...\n")
 * - Outgoing: Telemetry, Odometry, IMU, Sensors, Diagnostics, etc.
 *
 * @file   SerialProtocol.h
 * @author Assiut Robotics Team
 * @date   2026
 * ============================================================================
 */

#ifndef SERIALPROTOCOL_H
#define SERIALPROTOCOL_H

#include "Config.h"
#include <Arduino.h>

/**
 * @brief Parsed command structure for wheel velocities.
 */
struct ParsedCommand {
    char wheel;      ///< 'r' for right, 'l' for left
    char sign;       ///< 'p' for positive, 'n' for negative
    double velocity; ///< Velocity magnitude in rad/s
    bool valid;      ///< True if command was successfully parsed
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
        READING_COMMAND,
        COMPLETE
    };

    SerialProtocol();
    void begin();

    /**
     * @brief Process incoming serial data
     * @param right_vel Output: right wheel command velocity
     * @param left_vel Output: left wheel command velocity
     * @return true if a complete velocity command packet was received
     */
    bool processInput(double& right_vel, double& left_vel);

    /**
     * @brief Check if an extended command was received.
     * @param cmd_buf Buffer to store the command
     * @param buf_size Size of the buffer
     * @return true if a command is available
     */
    bool getExtendedCommand(char* cmd_buf, size_t buf_size);

    void sendTelemetry(double right_vel, double left_vel) const;
    void sendOdometry(double x, double y, double theta) const;
    void sendIMU(double yaw, double pitch, double roll) const;
    void sendProximity(const uint16_t* values, uint8_t count) const;
    void sendMetalDetect(bool detected) const;
    void sendLiftState(const char* state_str, uint8_t magnet_mask) const;
    void sendStatus(const char* message) const;
    void sendError(uint8_t error_code, const char* message) const;

    unsigned long getLastCommandTime() const { return last_command_time_; }
    bool dataAvailable() const { return Serial.available() > 0; }

private:
    State parser_state_;
    char rx_buffer_[SerialConfig::RX_BUFFER_SIZE];
    size_t rx_index_;
    unsigned long last_command_time_;

    // Extended command state
    char cmd_buffer_[SerialConfig::CMD_BUFFER_SIZE];
    size_t cmd_index_;
    bool cmd_ready_;

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
    ParsedCommand parseSingleCommand(const char* wheel_char,
                                     const char* sign_char,
                                     const char* value_str);
    void processCompletePacket();
};

#endif // SERIALPROTOCOL_H