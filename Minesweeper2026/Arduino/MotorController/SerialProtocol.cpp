/**
 * ============================================================================
 * SERIALPROTOCOL.CPP - Serial Communication Protocol Implementation
 * ============================================================================
 */

#include "SerialProtocol.h"
#include <stdlib.h>
#include <string.h>

SerialProtocol::SerialProtocol()
    : parser_state_(State::WAITING_PREFIX),
      rx_index_(0),
      last_command_time_(0),
      cmd_index_(0),
      cmd_ready_(false),
      current_motor_(' '),
      current_sign_('p'),
      value_index_(0),
      pending_right_vel_(0.0f),
      pending_left_vel_(0.0f),
      pending_gripper_vel_(0.0f),
      right_received_(false),
      left_received_(false),
      gripper_received_(false) {
    memset(rx_buffer_, 0, sizeof(rx_buffer_));
    memset(value_buffer_, 0, sizeof(value_buffer_));
    memset(cmd_buffer_, 0, sizeof(cmd_buffer_));
}

void SerialProtocol::begin() {
    Serial.begin(SerialConfig::BAUD_RATE);
    while (!Serial) { ; }
    last_command_time_ = millis();
}

bool SerialProtocol::processInput(float& right_vel, float& left_vel, float& gripper_vel) {
    bool packet_complete = false;

    while (Serial.available() > 0 && !packet_complete) {
        char c = Serial.read();

        switch (parser_state_) {
        case State::WAITING_PREFIX:
            if (c == 'r' || c == 'l' || c == 'g') {
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
            }
            break;

        case State::READING_VALUE:
            if (c == ',' || c == '\n' || c == '\r') {
                float velocity = atof(value_buffer_);
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
                }

                // Consider packet complete if we got at least right and left (gripper is optional or together)
                // Wait, if packet is "rp02.50,ln01.30,gp01.00,", it ends with ',' followed by nothing, or '\n'
                // Let's say if we get '\n', we process. If we get ',', we go to WAITING_PREFIX.
                if (c == '\n' || c == '\r') {
                    if (right_received_ && left_received_) { // Gripper is optional
                        right_vel = pending_right_vel_;
                        left_vel = pending_left_vel_;
                        if (gripper_received_) gripper_vel = pending_gripper_vel_;
                        processCompletePacket();
                        packet_complete = true;
                    } else {
                        resetParser();
                    }
                } else {
                    parser_state_ = State::WAITING_PREFIX;
                }
            } else if ((c >= '0' && c <= '9') || c == '.') {
                if (value_index_ < sizeof(value_buffer_) - 1) {
                    value_buffer_[value_index_++] = c;
                    value_buffer_[value_index_] = '\0';
                } else {
                    resetParser();
                }
            } else {
                resetParser();
            }
            break;

        case State::READING_COMMAND:
            if (c == '\n' || c == '\r') {
                if (cmd_index_ > 0) {
                    cmd_buffer_[cmd_index_] = '\0';
                    cmd_ready_ = true;
                }
                resetParser();
            } else {
                if (cmd_index_ < SerialConfig::CMD_BUFFER_SIZE - 1) {
                    cmd_buffer_[cmd_index_++] = c;
                } else {
                    resetParser();
                }
            }
            break;

        case State::COMPLETE:
            resetParser();
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
    parser_state_ = State::COMPLETE;
    right_received_ = false;
    left_received_ = false;
    gripper_received_ = false;
}

void SerialProtocol::resetParser() {
    parser_state_ = State::WAITING_PREFIX;
    rx_index_ = 0;
    right_received_ = false;
    left_received_ = false;
    gripper_received_ = false;
}

void SerialProtocol::resetValueBuffer() {
    value_index_ = 0;
    value_buffer_[0] = '\0';
}

void SerialProtocol::sendTelemetry(float right_vel, float left_vel, float gripper_vel) const {
    char buffer[SerialConfig::TX_BUFFER_SIZE];

    char r_sign = (right_vel >= 0) ? 'p' : 'n';
    char l_sign = (left_vel >= 0)  ? 'p' : 'n';
    char g_sign = (gripper_vel >= 0) ? 'p' : 'n';

    char r_val[10], l_val[10], g_val[10];
    dtostrf(abs(right_vel), 5, 2, r_val);
    dtostrf(abs(left_vel), 5, 2, l_val);
    dtostrf(abs(gripper_vel), 5, 2, g_val);

    snprintf(buffer, sizeof(buffer), "r%c%s,l%c%s,g%c%s,", r_sign, r_val, l_sign, l_val, g_sign, g_val);
    Serial.println(buffer);
}

void SerialProtocol::sendOdometry(float x, float y, float theta) const {
    char buffer[SerialConfig::TX_BUFFER_SIZE];
    char x_val[10], y_val[10], t_val[10];

    dtostrf(x, 6, 4, x_val);
    dtostrf(y, 6, 4, y_val);
    dtostrf(theta, 6, 4, t_val);

    snprintf(buffer, sizeof(buffer), "O:%s,%s,%s", x_val, y_val, t_val);
    Serial.println(buffer);
}

void SerialProtocol::sendIMU(float yaw, float pitch, float roll, float ax, float ay, float az) const {
    char buffer[SerialConfig::TX_BUFFER_SIZE];
    char y_val[8], p_val[8], r_val[8];

    dtostrf(yaw, 5, 1, y_val);
    dtostrf(pitch, 5, 1, p_val);
    dtostrf(roll, 5, 1, r_val);

    snprintf(buffer, sizeof(buffer), "I:%s,%s,%s", y_val, p_val, r_val);
    Serial.println(buffer);
}

void SerialProtocol::sendProximity(const uint16_t* values, uint8_t count) const {
    if (count != 5) return;
    char buffer[SerialConfig::TX_BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "P:%u,%u,%u,%u,%u",
             values[0], values[1], values[2], values[3], values[4]);
    Serial.println(buffer);
}

void SerialProtocol::sendMetalDetect(bool detected) const {
    Serial.print(F("M:"));
    Serial.println(detected ? '1' : '0');
}

void SerialProtocol::sendLiftState(const char* state_str, uint8_t magnet_mask) const {
    Serial.print(F("L:"));
    Serial.print(state_str);
    Serial.print(F(","));
    Serial.println(magnet_mask, HEX);
}

void SerialProtocol::sendDiagnostics(uint16_t loop_hz, uint8_t cpu_percent, uint8_t fault_code) const {
    char buffer[SerialConfig::TX_BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "D:%u,%u,%u", loop_hz, cpu_percent, fault_code);
    Serial.println(buffer);
}

void SerialProtocol::sendStatus(const char* message) const {
    Serial.print(F("S:"));
    Serial.println(message);
}

void SerialProtocol::sendError(uint8_t error_code, const char* message) const {
    Serial.print(F("E:"));
    Serial.print(error_code);
    Serial.print(F(":"));
    Serial.println(message);
}