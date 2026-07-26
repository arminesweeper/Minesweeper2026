/**
 * ============================================================================
 * MOTORDRIVER.CPP - Cytron MDD10A Motor Driver Implementation
 * ============================================================================
 */

#include "MotorDriver.h"

MotorDriver::MotorDriver(const MotorPinConfig &config, bool inverted)
    : config_(config), inverted_(inverted), is_forward_(true),
      current_output_(0.0f) {}

void MotorDriver::begin() {
  pinMode(config_.pwm_pin, OUTPUT);
  pinMode(config_.dir_pin, OUTPUT);

  stop();
}

void MotorDriver::setOutput(float pwm) {
  // Constrain PWM to safe bounds
  pwm = constrain(pwm, -255.0f, 255.0f);
  current_output_ = pwm;

  // Determine direction based on sign
  bool forward = (pwm >= 0.0f);

  // Apply hardware inversion if configured
  if (inverted_) {
    forward = !forward;
  }

  // Update hardware
  setDirection(forward);
  writePWM(static_cast<uint8_t>(abs(pwm)));
}

void MotorDriver::stop() {
  current_output_ = 0.0f;
  writePWM(0);
  // Do not change direction pin when stopping to prevent H-bridge shoot-through
}

void MotorDriver::emergencyStop() {
  // Hard disable at the register/pin level immediately
  digitalWrite(config_.pwm_pin, LOW);
  current_output_ = 0.0f;
}

void MotorDriver::setDirection(bool forward) {
  if (is_forward_ != forward) {
    is_forward_ = forward;
    digitalWrite(config_.dir_pin, forward ? HIGH : LOW);
  }
}

void MotorDriver::writePWM(uint8_t pwm) {
  analogWrite(config_.pwm_pin, pwm);
}