/**
 * ============================================================================
 * SERIALPROTOCOL.H - Serial Communication Protocol (RPi <-> Arduino)
 * ============================================================================
 * Header matching SerialProtocol.cpp from the MotorController project.
 * If you still have the original SerialProtocol.h from MotorController, you can
 * use it instead as long as it declares the same members.
 *
 * RPi -> Arduino (one packet per line, comma separated, trailing comma allowed)
 *     rp12.34,lp12.34,gp00.00,sp90.00,\n
 *     r / l : right / left wheel velocity (rad/s), 'p' = +, 'n' = -
 *     g     : gripper command (optional token)
 *     s     : servo angle in degrees (optional token)
 *     C<text>\n : extended command, e.g. "CSTAT", "CGRIP"
 *
 * Arduino -> RPi (one line each)
 *     rp0.00,lp0.00,go,        wheel velocities + gripper state letter
 *     O:x,y,theta   I:yaw,pitch,roll   P:a,b,c,d,e   B:volts   M:0|1
 *     L:state,mask  U:cm   D:hz,cpu,fault   S:text   E:code:text
 */
#ifndef SERIAL_PROTOCOL_H
#define SERIAL_PROTOCOL_H

#include <Arduino.h>

/* 1 = also read from / write to Serial2 (RPi on pins 16/17 instead of USB) */
#ifndef ENABLE_SERIAL2
#define ENABLE_SERIAL2 0
#endif

namespace SerialConfig {
    constexpr unsigned long BAUD_RATE        = 115200;
    constexpr uint8_t       RX_BUFFER_SIZE   = 16;
    constexpr uint8_t       CMD_BUFFER_SIZE  = 64;   /* longest "C..." extended command */
    constexpr uint8_t       VALUE_BUFFER_SIZE = 12;  /* one number, e.g. "12.34" */
    constexpr uint8_t       TX_BUFFER_SIZE   = 64;   /* one outgoing line */
}

class SerialProtocol {
public:
    SerialProtocol();

    void begin();

    /* Call continuously. Returns true when a complete r/l(/g/s) packet was parsed.
       gripper_vel is only changed if the packet carried a g token,
       servo_angle is -1 if the packet carried no s token. */
    bool processInput(float& right_vel, float& left_vel, float& gripper_vel, float& servo_angle);

    /* "C<text>" line received: copies <text> and returns true once */
    bool getExtendedCommand(char* cmd_buf, size_t buf_size);

    void sendTelemetry(float right_vel, float left_vel, char gripper_state) const;
    void sendOdometry(float x, float y, float theta) const;
    void sendIMU(float yaw, float pitch, float roll, float ax, float ay, float az) const;
    void sendProximity(const uint16_t* values, uint8_t count) const;
    void sendBattery(float voltage_mv) const;
    void sendMetalDetect(bool detected) const;
    void sendLiftState(const char* state_str, uint8_t magnet_mask) const;
    void sendUltrasonic(uint16_t distance_cm) const;
    void sendDiagnostics(uint16_t loop_hz, uint8_t cpu_percent, uint8_t fault_code) const;
    void sendStatus(const char* message) const;
    void sendError(uint8_t error_code, const char* message) const;

private:
    enum class State : uint8_t {
        WAITING_PREFIX,
        READING_DIRECTION,
        READING_VALUE,
        READING_COMMAND,
        COMPLETE
    };

    void processCompletePacket();
    void resetParser();
    void resetValueBuffer();

    /* declared in the same order as the constructor initialiser list */
    State         parser_state_;
    uint8_t       rx_index_;
    unsigned long last_command_time_;
    unsigned long last_rx_byte_time_;
    Stream*       active_stream_;
    uint8_t       cmd_index_;
    bool          cmd_ready_;
    char          current_motor_;
    char          current_sign_;
    uint8_t       value_index_;
    float         pending_right_vel_;
    float         pending_left_vel_;
    float         pending_gripper_vel_;
    float         pending_servo_angle_;
    bool          right_received_;
    bool          left_received_;
    bool          gripper_received_;
    bool          servo_received_;

    char rx_buffer_[SerialConfig::RX_BUFFER_SIZE];
    char value_buffer_[SerialConfig::VALUE_BUFFER_SIZE];
    char cmd_buffer_[SerialConfig::CMD_BUFFER_SIZE];
};

#endif /* SERIAL_PROTOCOL_H */
