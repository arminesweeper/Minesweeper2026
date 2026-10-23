/**
 * ============================================================================
 * SERIALPROTOCOL.CPP - Serial Communication Protocol Implementation
 * ============================================================================
 */

#include "SerialProtocol.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Reliable send helpers */
static void sendStringToPort(HardwareSerial& port, const char* str) {
    size_t len = strlen(str) + 2; // account for \r\n
    if (port.availableForWrite() >= len) {
        port.println(str);
    }
}

static void sendPrefixAndCharToPort(HardwareSerial& port, const __FlashStringHelper* prefix, char c) {
    port.print(prefix);
    port.println(c);
}

static void sendPrefixAndStringToPort(HardwareSerial& port, const __FlashStringHelper* prefix, const char* str) {
    port.print(prefix);
    port.println(str);
}

static void sendLiftStateToPort(HardwareSerial& port, const char* state_str, uint8_t magnet_mask, bool top_sw, bool bot_sw) {
    port.print(F("L:"));
    port.print(state_str);
    port.print(F(","));
    port.print(magnet_mask, HEX);
    port.print(F(","));
    port.print(top_sw ? '1' : '0');
    port.print(F(","));
    port.println(bot_sw ? '1' : '0');
}

static void sendErrorToPort(HardwareSerial& port, uint8_t error_code, const char* message) {
    port.print(F("E:"));
    port.print(error_code);
    port.print(F(":"));
    port.println(message);
}

static void broadcastString(const char* str) {
    sendStringToPort(Serial, str);
#if ENABLE_SERIAL2
    sendStringToPort(Serial2, str);
#endif
}

SerialProtocol::SerialProtocol()
    : last_command_time_(0),
      new_command_(false),
      cmd_ready_(false),
      pending_right_vel_(0.0f),
      pending_left_vel_(0.0f),
      pending_gripper_vel_(0.0f),
      pending_servo_angle_(-1.0f),
      right_received_(false),
      left_received_(false),
      gripper_received_(false),
      servo_received_(false) {
    memset(&parser_serial_, 0, sizeof(parser_serial_));
#if ENABLE_SERIAL2
    memset(&parser_serial2_, 0, sizeof(parser_serial2_));
#endif
    memset(cmd_buffer_, 0, sizeof(cmd_buffer_));
}

void SerialProtocol::begin() {
    Serial.begin(SerialConfig::BAUD_RATE);
#if ENABLE_SERIAL2
    Serial2.begin(SerialConfig::BAUD_RATE);
#endif
    last_command_time_ = millis();
}

void SerialProtocol::readStream(Stream& port, StreamParserState& parser) {
    while (port.available() > 0) {
        char c = static_cast<char>(port.read());

        if (c == ',' || c == '\n' || c == '\r') {
            if (parser.token_index > 0) {
                parser.token_buffer[parser.token_index] = '\0';
                processToken(parser.token_buffer);
                parser.token_index = 0;
            }
            continue;
        }

        if (parser.token_index < sizeof(parser.token_buffer) - 1) {
            parser.token_buffer[parser.token_index++] = c;
        } else {
            parser.token_index = 0;
        }
    }
}

void SerialProtocol::processToken(const char* token) {
    if (token[0] == '\0') return;

    // Direct wheel velocity tokens from ROS or joysticks: rp1.00, rn0.50, lp1.00, ln0.50
    if ((token[0] == 'r' || token[0] == 'l') && (token[1] == 'p' || token[1] == 'n')) {
        float val = static_cast<float>(atof(token + 2));
        if (token[1] == 'n') val = -val;
        if (token[0] == 'r') {
            pending_right_vel_ = val;
            right_received_ = true;
        } else {
            pending_left_vel_ = val;
            left_received_ = true;
        }
        last_command_time_ = millis();
        new_command_ = true;
        return;
    }

    // Camera servo direction / velocity token: sp, sn
    if (token[0] == 's' && (token[1] == 'p' || token[1] == 'n')) {
        float val = static_cast<float>(atof(token + 2));
        if (val >= 0.01f) {
            pending_servo_angle_ = (token[1] == 'p') ? val : -val;
            servo_received_ = true;
            new_command_ = true;
        }
        last_command_time_ = millis();
        return;
    }

    // Gripper velocity token from ROS: gp, gn
    if (token[0] == 'g' && (token[1] == 'p' || token[1] == 'n')) {
        float val = static_cast<float>(atof(token + 2));
        if (token[1] == 'n') val = -val;
        pending_gripper_vel_ = val;
        gripper_received_ = true;
        last_command_time_ = millis();
        new_command_ = true;
        return;
    }

    // Additive tokens matching RobotTest2
    if (strcmp(token, "cycle:1") == 0) {
        pending_gripper_vel_ = 1.0f; // Triggers cycle in SystemManager
        gripper_received_ = true;
        last_command_time_ = millis();
        new_command_ = true;
        return;
    }

    if (strncmp(token, "CSERVO:", 7) == 0) {
        pending_servo_angle_ = static_cast<float>(atof(token + 7));
        servo_received_ = true;
        last_command_time_ = millis();
        new_command_ = true;
        return;
    }

    if (strncmp(token, "SERVO:", 6) == 0) {
        pending_servo_angle_ = static_cast<float>(atof(token + 6));
        servo_received_ = true;
        last_command_time_ = millis();
        new_command_ = true;
        return;
    }

    // Direct single-letter drive commands (w, a, s, d, x)
    if (strlen(token) == 1) {
        const float v = DefaultParams::MAX_VELOCITY * 0.5f;
        switch (token[0]) {
            case 'w': case 'W':
                pending_right_vel_ = +v; pending_left_vel_ = +v;
                right_received_ = true; left_received_ = true;
                last_command_time_ = millis(); new_command_ = true;
                break;
            case 's': case 'S':
                pending_right_vel_ = -v; pending_left_vel_ = -v;
                right_received_ = true; left_received_ = true;
                last_command_time_ = millis(); new_command_ = true;
                break;
            case 'a': case 'A':
                pending_right_vel_ = +v; pending_left_vel_ = -v;
                right_received_ = true; left_received_ = true;
                last_command_time_ = millis(); new_command_ = true;
                break;
            case 'd': case 'D':
                pending_right_vel_ = -v; pending_left_vel_ = +v;
                right_received_ = true; left_received_ = true;
                last_command_time_ = millis(); new_command_ = true;
                break;
            case 'x': case 'X':
                pending_right_vel_ = 0.0f; pending_left_vel_ = 0.0f;
                right_received_ = true; left_received_ = true;
                last_command_time_ = millis(); new_command_ = true;
                break;
            default:
                break;
        }
        return;
    }

    // Extended command starting with 'C' (e.g. CESTOP, CCLEAR, etc.)
    if (token[0] == 'C') {
        strlcpy(cmd_buffer_, token + 1, sizeof(cmd_buffer_));
        cmd_ready_ = true;
        last_command_time_ = millis();
        return;
    }
}

bool SerialProtocol::processInput(float& right_vel, float& left_vel, float& gripper_vel, bool& has_gripper_cmd, float& servo_angle) {
    new_command_ = false;
    readStream(Serial, parser_serial_);
#if ENABLE_SERIAL2
    readStream(Serial2, parser_serial2_);
#endif

    if (new_command_ || right_received_ || left_received_) {
        right_vel = pending_right_vel_;
        left_vel = pending_left_vel_;
        if (gripper_received_) {
            gripper_vel = pending_gripper_vel_;
            has_gripper_cmd = true;
            gripper_received_ = false;
        } else {
            has_gripper_cmd = false;
        }
        if (servo_received_) {
            servo_angle = pending_servo_angle_;
            servo_received_ = false;
        } else {
            servo_angle = -1.0f;
        }
        right_received_ = false;
        left_received_ = false;
        return true;
    }
    has_gripper_cmd = false;
    return false;
}

bool SerialProtocol::getExtendedCommand(char* cmd_buf, size_t buf_size) {
    if (cmd_ready_) {
        strlcpy(cmd_buf, cmd_buffer_, buf_size);
        cmd_ready_ = false;
        return true;
    }
    return false;
}

void SerialProtocol::sendTelemetry(float right_vel, float left_vel, bool proximity, bool metal,
                                   char gripper_state, int32_t enc_right, int32_t enc_left) const {
    char buffer[SerialConfig::TX_BUFFER_SIZE];

    char r_sign = (right_vel >= 0.0f) ? 'p' : 'n';
    char l_sign = (left_vel >= 0.0f)  ? 'p' : 'n';

    char r_val[10], l_val[10];
    dtostrf(fabsf(right_vel), 4, 2, r_val);
    dtostrf(fabsf(left_vel), 4, 2, l_val);

    // Strip leading spaces from dtostrf output
    char* r_p = r_val;
    while (*r_p == ' ') r_p++;
    char* l_p = l_val;
    while (*l_p == ' ') l_p++;

    char p_char = proximity ? '1' : '0';
    char m_char = metal ? '1' : '0';

    // Format: rp0.00,lp0.00,p0,m0,gc,er:0,el:0,
    snprintf(buffer, sizeof(buffer), "r%c%s,l%c%s,p%c,m%c,g%c,er:%ld,el:%ld,",
             r_sign, r_p, l_sign, l_p, p_char, m_char, gripper_state, (long)enc_right, (long)enc_left);
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

void SerialProtocol::sendLiftState(const char* state_str, uint8_t magnet_mask, bool top_sw, bool bot_sw) const {
    sendLiftStateToPort(Serial, state_str, magnet_mask, top_sw, bot_sw);
#if ENABLE_SERIAL2
    sendLiftStateToPort(Serial2, state_str, magnet_mask, top_sw, bot_sw);
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