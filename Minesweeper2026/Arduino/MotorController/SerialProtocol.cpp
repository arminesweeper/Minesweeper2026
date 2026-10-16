/**
 * ============================================================================
 * SERIALPROTOCOL.CPP - Serial Communication Protocol Implementation
 * ============================================================================
 */

#include "SerialProtocol.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Safe non-blocking send helpers: only writes if TX buffer has enough room */
static void sendStringToPort(HardwareSerial& port, const char* str) {
    size_t len = strlen(str) + 2;
    if (port.availableForWrite() >= static_cast<int>(len)) {
        port.println(str);
    }
}

static void sendPrefixAndCharToPort(HardwareSerial& port, const __FlashStringHelper* prefix, char c) {
    if (port.availableForWrite() >= 6) {
        port.print(prefix);
        port.println(c);
    }
}

static void sendPrefixAndStringToPort(HardwareSerial& port, const __FlashStringHelper* prefix, const char* str) {
    size_t len = strlen(str) + 6;
    if (port.availableForWrite() >= static_cast<int>(len)) {
        port.print(prefix);
        port.println(str);
    }
}

static void sendLiftStateToPort(HardwareSerial& port, const char* state_str, uint8_t magnet_mask) {
    size_t len = strlen(state_str) + 8;
    if (port.availableForWrite() >= static_cast<int>(len)) {
        port.print(F("L:"));
        port.print(state_str);
        port.print(F(","));
        port.println(magnet_mask, HEX);
    }
}

static void sendErrorToPort(HardwareSerial& port, uint8_t error_code, const char* message) {
    size_t len = strlen(message) + 8;
    if (port.availableForWrite() >= static_cast<int>(len)) {
        port.print(F("E:"));
        port.print(error_code);
        port.print(F(":"));
        port.println(message);
    }
}

static void broadcastString(const char* str) {
    sendStringToPort(Serial, str);
#if ENABLE_SERIAL2
    sendStringToPort(Serial2, str);
#endif
}

SerialProtocol::SerialProtocol()
    : parser_state_(State::WAITING_PREFIX),
      rx_index_(0),
      last_command_time_(0),
      last_rx_byte_time_(0),
      active_stream_(nullptr),
      cmd_index_(0),
      cmd_ready_(false),
      current_motor_(' '),
      current_sign_('p'),
      value_index_(0),
      pending_right_vel_(0.0f),
      pending_left_vel_(0.0f),
      pending_gripper_vel_(0.0f),
      pending_servo_angle_(-1.0f),
      right_received_(false),
      left_received_(false),
      gripper_received_(false),
      servo_received_(false) {
    memset(rx_buffer_, 0, sizeof(rx_buffer_));
    memset(value_buffer_, 0, sizeof(value_buffer_));
    memset(cmd_buffer_, 0, sizeof(cmd_buffer_));
}

void SerialProtocol::begin() {
    Serial.begin(SerialConfig::BAUD_RATE);
#if ENABLE_SERIAL2
    Serial2.begin(SerialConfig::BAUD_RATE);
#endif
    last_command_time_ = millis();
    last_rx_byte_time_ = millis();
    active_stream_ = nullptr;
}

bool SerialProtocol::processInput(float& right_vel, float& left_vel, float& gripper_vel, float& servo_angle) {
    bool packet_complete = false;
    unsigned long now = millis();

    // Reset stream latch if more than 100 ms elapsed between bytes
    if (active_stream_ != nullptr && (now - last_rx_byte_time_ > 100)) {
        resetParser();
        active_stream_ = nullptr;
    }

    // Latch active stream so bytes from Serial and Serial2 never interleave
    if (active_stream_ == nullptr) {
        if (Serial.available() > 0) active_stream_ = &Serial;
#if ENABLE_SERIAL2
        else if (Serial2.available() > 0) active_stream_ = &Serial2;
#endif
    }

    while (active_stream_ != nullptr && active_stream_->available() > 0 && !packet_complete) {
        char c = active_stream_->read();
        last_rx_byte_time_ = millis();

        switch (parser_state_) {
        case State::WAITING_PREFIX:
            if (c == '\n' || c == '\r') {
                if (right_received_ && left_received_) {
                    right_vel = pending_right_vel_;
                    left_vel = pending_left_vel_;
                    if (gripper_received_) gripper_vel = pending_gripper_vel_;
                    if (servo_received_) servo_angle = pending_servo_angle_;
                    else servo_angle = -1.0f;
                    processCompletePacket();
                    packet_complete = true;
                    active_stream_ = nullptr;
                } else {
                    resetParser();
                    active_stream_ = nullptr;
                }
            } else if (c == 'r' || c == 'l' || c == 'g' || c == 's') {
                // If a new 'r' arrives while a full packet is already parsed (streamed without newline)
                if (c == 'r' && right_received_ && left_received_) {
                    right_vel = pending_right_vel_;
                    left_vel = pending_left_vel_;
                    if (gripper_received_) gripper_vel = pending_gripper_vel_;
                    if (servo_received_) servo_angle = pending_servo_angle_;
                    else servo_angle = -1.0f;
                    processCompletePacket();
                    packet_complete = true;
                }
                current_motor_ = c;
                parser_state_ = State::READING_DIRECTION;
            } else if (c == 'C') {
                cmd_index_ = 0;
                parser_state_ = State::READING_COMMAND;
            }
            break;

        case State::READING_DIRECTION:
            if (c == 'p' || c == 'n') {
                current_sign_ = c;
                resetValueBuffer();
                parser_state_ = State::READING_VALUE;
            } else {
                resetParser();
                active_stream_ = nullptr;
            }
            break;

        case State::READING_VALUE:
            if (c == ',' || c == '\n' || c == '\r') {
                float velocity = static_cast<float>(atof(value_buffer_));
                if (current_sign_ == 'n') {
                    velocity = -velocity;
                }

                if (current_motor_ == 'r') {
                    pending_right_vel_ = velocity;
                    right_received_ = true;
                } else if (current_motor_ == 'l') {
                    pending_left_vel_ = velocity;
                    left_received_ = true;
                } else if (current_motor_ == 'g') {
                    pending_gripper_vel_ = velocity;
                    gripper_received_ = true;
                } else if (current_motor_ == 's') {
                    pending_servo_angle_ = velocity;
                    servo_received_ = true;
                }

                // If newline or end of packet reached
                if (c == '\n' || c == '\r') {
                    if (right_received_ && left_received_) {
                        right_vel = pending_right_vel_;
                        left_vel = pending_left_vel_;
                        if (gripper_received_) gripper_vel = pending_gripper_vel_;
                        if (servo_received_) servo_angle = pending_servo_angle_;
                        else servo_angle = -1.0f;
                        processCompletePacket();
                        packet_complete = true;
                        active_stream_ = nullptr;
                    } else {
                        resetParser();
                        active_stream_ = nullptr;
                    }
                } else {
                    // Delimiter is comma ','
                    // If gripper or servo was already received, or if both wheels are ready and
                    // the stream buffer is empty after brief wait, complete packet
                    if (right_received_ && left_received_ && (gripper_received_ || servo_received_)) {
                        right_vel = pending_right_vel_;
                        left_vel = pending_left_vel_;
                        gripper_vel = pending_gripper_vel_;
                        if (servo_received_) servo_angle = pending_servo_angle_;
                        else servo_angle = -1.0f;
                        processCompletePacket();
                        packet_complete = true;
                        active_stream_ = nullptr;
                    } else {
                        parser_state_ = State::WAITING_PREFIX;
                    }
                }
            } else if ((c >= '0' && c <= '9') || c == '.') {
                if (value_index_ < sizeof(value_buffer_) - 1) {
                    value_buffer_[value_index_++] = c;
                    value_buffer_[value_index_] = '\0';
                } else {
                    resetParser();
                    active_stream_ = nullptr;
                }
            } else {
                resetParser();
                active_stream_ = nullptr;
            }
            break;

        case State::READING_COMMAND:
            if (c == '\n' || c == '\r') {
                if (cmd_index_ > 0) {
                    cmd_buffer_[cmd_index_] = '\0';
                    cmd_ready_ = true;
                }
                resetParser();
                active_stream_ = nullptr;
            } else {
                if (cmd_index_ < SerialConfig::CMD_BUFFER_SIZE - 1) {
                    cmd_buffer_[cmd_index_++] = c;
                } else {
                    resetParser();
                    active_stream_ = nullptr;
                }
            }
            break;

        case State::COMPLETE:
            resetParser();
            active_stream_ = nullptr;
            break;
        }
    }

    return packet_complete;
}

bool SerialProtocol::getExtendedCommand(char* cmd_buf, size_t buf_size) {
    if (cmd_ready_) {
        strlcpy(cmd_buf, cmd_buffer_, buf_size);
        cmd_ready_ = false;
        return true;
    }
    return false;
}

void SerialProtocol::processCompletePacket() {
    last_command_time_ = millis();
    parser_state_ = State::WAITING_PREFIX;
    right_received_ = false;
    left_received_ = false;
    gripper_received_ = false;
    servo_received_ = false;
    pending_servo_angle_ = -1.0f;
}

void SerialProtocol::resetParser() {
    parser_state_ = State::WAITING_PREFIX;
    rx_index_ = 0;
    right_received_ = false;
    left_received_ = false;
    gripper_received_ = false;
    servo_received_ = false;
    pending_servo_angle_ = -1.0f;
}

void SerialProtocol::resetValueBuffer() {
    value_index_ = 0;
    value_buffer_[0] = '\0';
}

void SerialProtocol::sendTelemetry(float right_vel, float left_vel, char gripper_state) const {
    char buffer[SerialConfig::TX_BUFFER_SIZE];

    char r_sign = (right_vel >= 0) ? 'p' : 'n';
    char l_sign = (left_vel >= 0)  ? 'p' : 'n';

    char r_val[10], l_val[10];
    dtostrf(fabsf(right_vel), 4, 2, r_val);
    dtostrf(fabsf(left_vel), 4, 2, l_val);

    // Strip leading spaces from dtostrf output
    char* r_p = r_val;
    while (*r_p == ' ') r_p++;
    char* l_p = l_val;
    while (*l_p == ' ') l_p++;

    // Format: rp0.00,lp0.00,go,
    snprintf(buffer, sizeof(buffer), "r%c%s,l%c%s,g%c,", r_sign, r_p, l_sign, l_p, gripper_state);
    broadcastString(buffer);
}

void SerialProtocol::sendOdometry(float x, float y, float theta) const {
    char buffer[SerialConfig::TX_BUFFER_SIZE];
    char x_val[10], y_val[10], t_val[10];

    dtostrf(x, 6, 4, x_val);
    dtostrf(y, 6, 4, y_val);
    dtostrf(theta, 6, 4, t_val);

    snprintf(buffer, sizeof(buffer), "O:%s,%s,%s", x_val, y_val, t_val);
    broadcastString(buffer);
}

void SerialProtocol::sendIMU(float yaw, float pitch, float roll, float ax, float ay, float az) const {
    (void)ax; (void)ay; (void)az;
    char buffer[SerialConfig::TX_BUFFER_SIZE];
    char y_val[8], p_val[8], r_val[8];

    dtostrf(yaw, 5, 1, y_val);
    dtostrf(pitch, 5, 1, p_val);
    dtostrf(roll, 5, 1, r_val);

    snprintf(buffer, sizeof(buffer), "I:%s,%s,%s", y_val, p_val, r_val);
    broadcastString(buffer);
}

void SerialProtocol::sendProximity(const uint16_t* values, uint8_t count) const {
    if (count != 5) return;
    char buffer[SerialConfig::TX_BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "P:%u,%u,%u,%u,%u",
             values[0], values[1], values[2], values[3], values[4]);
    broadcastString(buffer);
}

void SerialProtocol::sendBattery(float voltage_mv) const {
    char buffer[SerialConfig::TX_BUFFER_SIZE];
    char v_str[10];
    dtostrf(voltage_mv / 1000.0f, 4, 2, v_str);
    snprintf(buffer, sizeof(buffer), "B:%s", v_str);
    broadcastString(buffer);
}

void SerialProtocol::sendMetalDetect(bool detected) const {
    sendPrefixAndCharToPort(Serial, F("M:"), detected ? '1' : '0');
#if ENABLE_SERIAL2
    sendPrefixAndCharToPort(Serial2, F("M:"), detected ? '1' : '0');
#endif
}

void SerialProtocol::sendLiftState(const char* state_str, uint8_t magnet_mask) const {
    sendLiftStateToPort(Serial, state_str, magnet_mask);
#if ENABLE_SERIAL2
    sendLiftStateToPort(Serial2, state_str, magnet_mask);
#endif
}

void SerialProtocol::sendUltrasonic(uint16_t distance_cm) const {
    char buffer[SerialConfig::TX_BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "U:%u", distance_cm);
    broadcastString(buffer);
}

void SerialProtocol::sendDiagnostics(uint16_t loop_hz, uint8_t cpu_percent, uint8_t fault_code) const {
    char buffer[SerialConfig::TX_BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "D:%u,%u,%u", loop_hz, cpu_percent, fault_code);
    broadcastString(buffer);
}

void SerialProtocol::sendStatus(const char* message) const {
    sendPrefixAndStringToPort(Serial, F("S:"), message);
#if ENABLE_SERIAL2
    sendPrefixAndStringToPort(Serial2, F("S:"), message);
#endif
}

void SerialProtocol::sendError(uint8_t error_code, const char* message) const {
    sendErrorToPort(Serial, error_code, message);
#if ENABLE_SERIAL2
    sendErrorToPort(Serial2, error_code, message);
#endif
}