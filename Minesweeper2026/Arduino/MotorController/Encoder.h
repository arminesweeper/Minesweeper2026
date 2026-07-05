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
 * Provides interrupt-driven quadrature encoder reading with:
 * - Direction detection via Phase B state
 * - Atomic pulse count access
 * - Velocity calculation
 * - Error detection
 */
class Encoder {
public:
  /**
   * @brief Construct a new Encoder object
   * @param config Pin configuration
   @param direction_inverted Set true if encoder counts opposite to wheel
   direction
   */
  explicit Encoder(const EncoderPinConfig &config,
                   bool direction_inverted = false);

  /**
   * @brief Initialize encoder pins and attach interrupt
   * @param isr Pointer to the ISR function for this encoder
   */
  void begin(void (*isr)(void));

  /**
   * @brief Get pulse count and reset counter (atomic operation)
   * @return Number of pulses since last read
   */
  int32_t getPulsesAndReset();

  /**
   * @brief Get current pulse count without resetting
   * @return Current pulse count
   */
  int32_t getPulses() const;

  /**
   * @brief Reset pulse counter to zero
   */
  void reset();

  /**
   * @brief Calculate velocity from pulse count
   * @param pulses Pulse count over the sample period
   * @param dt_sec Sample period in seconds
   * @return Velocity in rad/s (signed)
   */
  double calculateVelocity(int32_t pulses, double dt_sec) const;

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
   * @brief Get total accumulated pulse count (for odometry)
   * @return Total pulses since last total reset
   */
  int32_t getTotalPulses() const { return total_pulse_count_; }

  /**
   * @brief Reset total pulse counter
   */
  void resetTotal() { total_pulse_count_ = 0; }

  // Static ISR callback targets - called from ISRs
  static void isrCallbackRight();
  static void isrCallbackLeft();

  // Static instance pointers for ISR access
  static Encoder *instance_right_;
  static Encoder *instance_left_;

private:
  EncoderPinConfig config_;
  bool direction_inverted_;

  volatile int32_t pulse_count_;       // Pulses since last read
  volatile int32_t total_pulse_count_; // Total pulses (for odometry)
  volatile int8_t direction_;          // Last detected direction
  volatile bool error_flag_;

  void handleInterrupt();
};

#endif // ENCODER_H