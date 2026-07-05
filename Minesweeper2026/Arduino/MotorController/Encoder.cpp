/**
 * ============================================================================
 * ENCODER.CPP - Quadrature Encoder Implementation
 * ============================================================================
 */

#include "Encoder.h"

// Initialize static instance pointers
Encoder *Encoder::instance_right_ = nullptr;
Encoder *Encoder::instance_left_ = nullptr;

Encoder::Encoder(const EncoderPinConfig &config, bool direction_inverted)
    : config_(config), direction_inverted_(direction_inverted), pulse_count_(0),
      total_pulse_count_(0), direction_(1), error_flag_(false) {}

void Encoder::begin(void (*isr)(void)) {
  pinMode(config_.phase_a_pin, INPUT_PULLUP);
  pinMode(config_.phase_b_pin, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(config_.phase_a_pin), isr, RISING);
}

int32_t Encoder::getPulsesAndReset() {
  int32_t pulses;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    pulses = pulse_count_;
    pulse_count_ = 0;
  }
  return pulses;
}

int32_t Encoder::getPulses() const {
  int32_t pulses;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { pulses = pulse_count_; }
  return pulses;
}

void Encoder::reset() {
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { pulse_count_ = 0; }
}

double Encoder::calculateVelocity(int32_t pulses, double dt_sec) const {
  if (dt_sec <= 0.0)
    return 0.0;

  // Calculate RPM from pulses
  double rpm =
      (static_cast<double>(pulses) / EncoderSpec::PPR) * (60.0 / dt_sec);

  // Convert to rad/s with direction
  double velocity_rads = rpm * EncoderSpec::RPM_TO_RADS;

  // Apply direction inversion if configured
  if (direction_inverted_) {
    velocity_rads = -velocity_rads;
  }

  return velocity_rads;
}

void Encoder::handleInterrupt() {
  // Read Phase B to determine direction
  if (digitalRead(config_.phase_b_pin) == HIGH) {
    direction_ = 1;
    pulse_count_++;
    total_pulse_count_++;
  } else {
    direction_ = -1;
    pulse_count_--;
    total_pulse_count_--;
  }
}

// Static ISR callbacks - delegate to instance methods
void Encoder::isrCallbackRight() {
  if (instance_right_ != nullptr) {
    instance_right_->handleInterrupt();
  }
}

void Encoder::isrCallbackLeft() {
  if (instance_left_ != nullptr) {
    instance_left_->handleInterrupt();
  }
}

// Global ISR functions (required for attachInterrupt)
void rightEncoderISR() { Encoder::isrCallbackRight(); }

void leftEncoderISR() { Encoder::isrCallbackLeft(); }