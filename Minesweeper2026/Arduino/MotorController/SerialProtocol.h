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
    bool processInput(float& right_vel, float& left_vel, float& gripper_vel, float& servo_angle);
    bool processInput(float& right_vel, float& left_vel, float& gripper_vel) {
        float dummy_servo = -1.0f;
        return processInput(right_vel, left_vel, gripper_vel, dummy_servo);
    }

    bool getExtendedCommand(char* cmd_buf, size_t buf_size);

    /**
     * @brief Send high-rate telemetry compatible with ROS parser
     * Format: rp02.50,ln01.30,p0,m0,gc,er:123,el:456,
     */
    void sendTelemetry(float right_vel, float left_vel, bool proximity, bool metal,
                       char gripper_state, int32_t enc_right = 0, int32_t enc_left = 0) const;
    void sendTelemetry(float right_vel, float left_vel, char gripper_state) const {
        sendTelemetry(right_vel, left_vel, false, false, gripper_state, 0, 0);
    }
    
    void sendOdometry(float x, float y, float theta) const;
    void sendIMU(float yaw, float pitch, float roll, float ax, float ay, float az) const;
    void sendProximity(const uint16_t* values, uint8_t count) const;
    void sendBattery(float voltage_mv) const;
    void sendMetalDetect(bool detected) const;
    void sendLiftState(const char* state_str, uint8_t magnet_mask, bool top_sw = false, bool bot_sw = false) const;
    void sendUltrasonic(uint16_t distance_cm) const;
    
    void sendDiagnostics(uint16_t loop_hz, uint8_t cpu_percent, uint8_t fault_code) const;
    void sendStatus(const char* message) const;
    void sendError(uint8_t error_code, const char* message) const;

    unsigned long getLastCommandTime() const { return last_command_time_; }
#if ENABLE_SERIAL2
    bool dataAvailable() const { return Serial.available() > 0 || Serial2.available() > 0; }
#else
    bool dataAvailable() const { return Serial.available() > 0; }
#endif

private:
    unsigned long last_command_time_;

    // Token-based parser (matches RobotTest2)
    char token_buffer_[64];
    uint8_t token_index_;
    bool new_command_;

    // Extended command state
    char cmd_buffer_[SerialConfig::CMD_BUFFER_SIZE];
    bool cmd_ready_;

    // Target command values
    float pending_right_vel_;
    float pending_left_vel_;
    float pending_gripper_vel_;
    float pending_servo_angle_;
    bool right_received_;
    bool left_received_;
    bool gripper_received_;
    bool servo_received_;

    void readStream(Stream& port);
    void processToken(const char* token);
};

#endif // SERIALPROTOCOL_H