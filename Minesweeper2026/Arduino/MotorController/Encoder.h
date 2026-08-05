/**
 * ============================================================================
 * ENCODER.H - Quadrature Encoder Interface
 * ============================================================================
 */

#ifndef ENCODER_H
#define ENCODER_H

#include "Config.h"
#include <Arduino.h>
#include <util/atomic.h>

/**
 * @brief Encoder channel pin configuration
 */
struct EncoderPinConfig {
  uint8_t phase_a_pin;  // Must be an interrupt-capable pin
  uint8_t phase_b_pin;  // General digital I/O
  uint8_t interrupt_id; // For documentation/reference
};

/**
 * @brief Quadrature Encoder Reader
 *
 * Provides highly optimized interrupt-driven quadrature encoder reading.
 * Uses direct AVR port manipulation inside ISR to minimize execution latency.
 */
class Encoder {
public:
  /**
   * @brief Construct a new Encoder object
   * @param config Pin configuration
   * @param direction_inverted Set true if encoder counts opposite to wheel direction
   */
  explicit Encoder(const EncoderPinConfig &config, bool direction_inverted = false);

  /**
   * @brief Initialize encoder pins, resolve hardware registers, and attach interrupt
   * @param isr Pointer to the ISR function for this encoder
   */
  void begin(void (*isr)(void));

  /**
   * @brief Get current total pulse count continuously (atomic operation).
   * Used with signed delta arithmetic (current - last) to prevent overflow issues.
   * @return Current pulse count
   */
  int32_t getPulses() const;

  /**
   * @brief Atomically read pulse delta since last call and clear the counter.
   * @return Signed pulse count accumulated since previous getPulsesAndReset()
   */
  int32_t getPulsesAndReset();

  /**
   * @brief Reset pulse counter to zero
   */
  void reset();

  /**
   * @brief ISR body — public so SystemManager/global ISRs can call it.
   * Must remain ISR-safe: no floating point, no Serial, no heap.
   */
  void handleInterrupt();

  /**
   * @brief Calculate velocity from pulse delta
   * @param delta_pulses Pulses accumulated over the sample period
   * @param dt_sec Sample period in seconds
   * @param rad_per_pulse Conversion factor from pulses to radians
   * @return Velocity in rad/s
   */
  float calculateVelocity(int32_t delta_pulses, float dt_sec, float rad_per_pulse) const;

  /**
   * @brief Get last measured direction
   * @return 1 for positive, -1 for negative
   */
  int8_t getDirection() const { return direction_; }

  /**
   * @brief Check for potential encoder errors
   * @return true if an error condition is detected
   */
  bool hasError() const { return error_flag_; }

  /**
   * @brief Clear error flag
   */
  void clearError() { error_flag_ = false; }

  /**
   * @brief Update direction inverted flag at runtime (from EEPROM)
   */
  void setDirectionInverted(bool inverted) { direction_inverted_ = inverted; }

  // Static ISR callback targets - called from ISRs
  static void isrCallbackRight();
  static void isrCallbackLeft();

  // Static instance pointers for ISR access
  static Encoder *instance_right_;
  static Encoder *instance_left_;

private:
  EncoderPinConfig config_;
  bool direction_inverted_;

  volatile int32_t pulse_count_; // Continuous pulse count
  volatile int8_t direction_;    // Last detected direction
  volatile bool error_flag_;

  // Direct port manipulation variables for fast ISR execution
  volatile uint8_t* phase_b_reg_;
  uint8_t phase_b_mask_;
};

#endif // ENCODER_H