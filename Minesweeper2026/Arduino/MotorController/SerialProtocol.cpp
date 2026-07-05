/**
 * ============================================================================
 * SERIALPROTOCOL.CPP - Serial Protocol Implementation
 * ============================================================================
 */

#include "SerialProtocol.h"

SerialProtocol::SerialProtocol()
    : parser_state_(State::WAITING_PREFIX), rx_index_(0), last_command_time_(0),
      parsing_right_(true), current_sign_('p'), value_index_(0),
      pending_right_vel_(0.0), pending_left_vel_(0.0), right_received_(false),
      left_received_(false) {
  resetValueBuffer();
}

void SerialProtocol::begin() {
  Serial.begin(SerialConfig::BAUD_RATE);
  last_command_time_ = millis();

  // Send startup message
  Serial.println(F("MC:READY"));
  Serial.println(F("FMT:r[p|n]XX.XX,l[p|n]XX.XX,"));
}

bool SerialProtocol::processInput(double &right_vel, double &left_vel) {
  bool command_complete = false;

  while (Serial.available() > 0) {
    char chr = Serial.read();

    switch (parser_state_) {
    case State::WAITING_PREFIX:
      if (chr == 'r') {
        parsing_right_ = true;
        parser_state_ = State::READING_DIRECTION;
      } else if (chr == 'l') {
        parsing_right_ = false;
        parser_state_ = State::READING_DIRECTION;
      } else if (chr == '\n' || chr == '\r') {
        // Handle empty lines
      } else {
        // Unknown character, could be start of status command
        // For now, reset parser
      }
      break;

    case State::READING_DIRECTION:
      if (chr == 'p' || chr == 'n') {
        current_sign_ = chr;
        parser_state_ = State::READING_VALUE;
        resetValueBuffer();
      } else {
        // Invalid direction character
        resetParser();
      }
      break;

    case State::READING_VALUE:
      if (chr == ',') {
        // End of value
        value_buffer_[value_index_] = '\0';
        double velocity = atof(value_buffer_);

        if (parsing_right_) {
          pending_right_vel_ = (current_sign_ == 'p') ? velocity : -velocity;
          right_received_ = true;
        } else {
          pending_left_vel_ = (current_sign_ == 'p') ? velocity : -velocity;
          left_received_ = true;
        }

        parser_state_ = State::WAITING_PREFIX;
      } else if (chr == '\n' || chr == '\r') {
        // End of packet
        value_buffer_[value_index_] = '\0';
        double velocity = atof(value_buffer_);

        if (parsing_right_) {
          pending_right_vel_ = (current_sign_ == 'p') ? velocity : -velocity;
          right_received_ = true;
        } else {
          pending_left_vel_ = (current_sign_ == 'p') ? velocity : -velocity;
          left_received_ = true;
        }

        // Check if we have a complete command pair
        if (right_received_ && left_received_) {
          right_vel = pending_right_vel_;
          left_vel = pending_left_vel_;
          last_command_time_ = millis();
          command_complete = true;

          // Reset for next command
          right_received_ = false;
          left_received_ = false;
        }

        resetParser();
      } else if (value_index_ < 7) {
        // Accept digits and decimal point
        if ((chr >= '0' && chr <= '9') || chr == '.') {
          value_buffer_[value_index_++] = chr;
        } else {
          // Invalid character in value
          resetParser();
        }
      } else {
        // Value too long
        resetParser();
      }
      break;

    default:
      resetParser();
      break;
    }
  }

  return command_complete;
}

void SerialProtocol::sendTelemetry(double right_vel, double left_vel) const {
  // Format: r[p|n]X.XXX,l[p|n]X.XXX,
  Serial.print('r');
  Serial.print(right_vel >= 0 ? 'p' : 'n');
  Serial.print(abs(right_vel), 3);
  Serial.print(F(",l"));
  Serial.print(left_vel >= 0 ? 'p' : 'n');
  Serial.print(abs(left_vel), 3);
  Serial.println(',');
}

void SerialProtocol::sendOdometry(double x, double y, double theta) const {
  // Format: O:X.XXX,Y.XXX,T.XXX,
  Serial.print(F("O:"));
  Serial.print(x, 4);
  Serial.print(',');
  Serial.print(y, 4);
  Serial.print(',');
  Serial.print(theta, 4);
  Serial.println(',');
}

void SerialProtocol::sendStatus(const char *message) const {
  Serial.print(F("S:"));
  Serial.println(message);
}

void SerialProtocol::sendError(uint8_t error_code, const char *message) const {
  Serial.print(F("E:"));
  Serial.print(error_code);
  Serial.print(':');
  Serial.println(message);
}

void SerialProtocol::resetParser() {
  parser_state_ = State::WAITING_PREFIX;
  value_index_ = 0;
  resetValueBuffer();
}

void SerialProtocol::resetValueBuffer() {
  value_buffer_[0] = '0';
  value_buffer_[1] = '0';
  value_buffer_[2] = '.';
  value_buffer_[3] = '0';
  value_buffer_[4] = '0';
  value_buffer_[5] = '\0';
  value_index_ = 0;
}