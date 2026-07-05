/**
 * ============================================================================
 * MOTORDRIVER.CPP - Cytron MDD10A Motor Driver Implementation
 * ============================================================================
 */

#include "MotorDriver.h"

MotorDriver::MotorDriver(const MotorPinConfig &config, bool inverted)
    : config_(config), inverted_(inverted), is_forward_(true),
      current_output_(0.0) {}

void MotorDriver::begin() {
  pinMode(config_.pwm_pin, OUTPUT);
  pinMode(config_.dir_pin, OUTPUT);

  // Initialize to safe state
  digitalWrite(config_.pwm_pin, LOW);
  digitalWrite(config_.dir_pin, HIGH); // Default forward
  is_forward_ = true;
  current_output_ = 0.0;
}

void MotorDriver::setOutput(double pwm) {
  // Clamp to valid range
  if (pwm > 255.0)
    pwm = 255.0;
  if (pwm < -255.0)
    pwm = -255.0;

  current_output_ = pwm;

  if (pwm >= 0) {
    // Forward direction
    setDirection(true);
    writePWM(static_cast<uint8_t>(pwm));
  } else {
    // Reverse direction
    setDirection(false);
    writePWM(static_cast<uint8_t>(-pwm));
  }
}

void MotorDriver::stop() {
  current_output_ = 0.0;
  writePWM(0);
}

void MotorDriver::emergencyStop() {
  current_output_ = 0.0;
  digitalWrite(config_.pwm_pin, LOW);
  analogWrite(config_.pwm_pin, 0);
}

void MotorDriver::setDirection(bool forward) {
  // Apply inversion if configured
  bool physical_direction = inverted_ ? !forward : forward;

  if (physical_direction != is_forward_) {
    digitalWrite(config_.dir_pin, physical_direction ? HIGH : LOW);
    is_forward_ = physical_direction;
  }
}

void MotorDriver::writePWM(uint8_t pwm) { analogWrite(config_.pwm_pin, pwm); }