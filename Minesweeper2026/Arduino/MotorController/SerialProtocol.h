/**
 * ============================================================================
 * SERIALPROTOCOL.H - Serial Communication Protocol
 * ============================================================================
 */

#ifndef SERIALPROTOCOL_H
#define SERIALPROTOCOL_H

#include "Config.h"
#include <Arduino.h>

class SerialProtocol {
public:
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
     * @return true if a complete velocity command packet was received
     */
    bool processInput(float& right_vel, float& left_vel, float& gripper_vel);

    bool getExtendedCommand(char* cmd_buf, size_t buf_size);

    /**
     * @brief Send high-rate telemetry compatible with ROS parser
     * Format: rp02.50,ln01.30,gp01.00,
     */
    void sendTelemetry(float right_vel, float left_vel, float gripper_vel) const;
    
    void sendOdometry(float x, float y, float theta) const;
    void sendIMU(float yaw, float pitch, float roll, float ax, float ay, float az) const;
    void sendProximity(const uint16_t* values, uint8_t count) const;
    void sendMetalDetect(bool detected) const;
    void sendLiftState(const char* state_str, uint8_t magnet_mask) const;
    
    void sendDiagnostics(uint16_t loop_hz, uint8_t cpu_percent, uint8_t fault_code) const;
    void sendStatus(const char* message) const;
    void sendError(uint8_t error_code, const char* message) const;

    unsigned long getLastCommandTime() const { return last_command_time_; }
    bool dataAvailable() const { return Serial.available() > 0 || Serial2.available() > 0; }

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
    char current_motor_; // 'r', 'l', 'g'
    char current_sign_;
    char value_buffer_[8];
    size_t value_index_;

    // Pending command values
    float pending_right_vel_;
    float pending_left_vel_;
    float pending_gripper_vel_;
    bool right_received_;
    bool left_received_;
    bool gripper_received_;

    void resetParser();
    void resetValueBuffer();
    void processCompletePacket();
};

#endif // SERIALPROTOCOL_H