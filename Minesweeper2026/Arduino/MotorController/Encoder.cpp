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
      direction_(1), error_flag_(false), phase_b_reg_(nullptr), phase_b_mask_(0) {}

void Encoder::begin(void (*isr)(void)) {
  pinMode(config_.phase_a_pin, INPUT_PULLUP);
  pinMode(config_.phase_b_pin, INPUT_PULLUP);

  // Pre-calculate hardware port and bitmask for ultra-fast ISR execution
  phase_b_reg_ = portInputRegister(digitalPinToPort(config_.phase_b_pin));
  phase_b_mask_ = digitalPinToBitMask(config_.phase_b_pin);

  attachInterrupt(digitalPinToInterrupt(config_.phase_a_pin), isr, RISING);
}

int32_t Encoder::getPulses() const {
  int32_t pulses;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { 
      pulses = pulse_count_; 
  }
  return pulses;
}

int32_t Encoder::getPulsesAndReset() {
  int32_t pulses;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      pulses = pulse_count_;
      pulse_count_ = 0;
  }
  return pulses;
}

void Encoder::reset() {
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { 
      pulse_count_ = 0; 
  }
}

float Encoder::calculateVelocity(int32_t delta_pulses, float dt_sec, float rad_per_pulse) const {
  if (dt_sec <= 0.0f) {
    return 0.0f;
  }

  // Calculate rad/s directly without division by PPR or 60
  float velocity_rads = (static_cast<float>(delta_pulses) * rad_per_pulse) / dt_sec;

  // Apply direction inversion if configured
  if (direction_inverted_) {
    velocity_rads = -velocity_rads;
  }

  return velocity_rads;
}

void Encoder::handleInterrupt() {
  // ISR Rule: NO floating point, NO Serial, NO malloc.
  // Read Phase B using direct AVR register access (~2 clock cycles)
  if ((*phase_b_reg_ & phase_b_mask_) != 0) {
    direction_ = 1;
    pulse_count_++;
  } else {
    direction_ = -1;
    pulse_count_--;
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
