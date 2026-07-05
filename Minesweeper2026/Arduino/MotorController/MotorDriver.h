/**
 * ============================================================================
 * MOTORDRIVER.H - Cytron MDD10A Motor Driver Abstraction
 * ============================================================================
 */

#ifndef MOTORDRIVER_H
#define MOTORDRIVER_H

#include "Config.h"
#include <Arduino.h>


/**
 * @brief Motor driver channel configuration
 */
struct MotorPinConfig {
  uint8_t pwm_pin;
  uint8_t dir_pin;
};

/**
 * @brief Cytron MDD10A Motor Driver Interface
 *
 * Provides sign-magnitude motor control for the Cytron MDD10A Rev 2.0.
 * Supports bidirectional PWM output with proper direction pin control.
 */
class MotorDriver {
public:
  /**
   * @brief Construct a new Motor Driver object
   * @param config Pin configuration for this motor channel
   * @param inverted Set true if motor direction is physically reversed
   */
  explicit MotorDriver(const MotorPinConfig &config, bool inverted = false);

  /**
   * @brief Initialize motor driver pins
   */
  void begin();

  /**
   * @brief Set motor output with signed PWM value
   * @param pwm Signed PWM value (-255 to +255)
   *
   * Positive values = forward, Negative values = reverse
   * Automatically handles DIR pin state for sign-magnitude mode.
   */
  void setOutput(double pwm);

  /**
   * @brief Immediately stop the motor
   * Sets PWM to 0 and holds current direction
   */
  void stop();

  /**
   * @brief Emergency stop - disables output completely
   */
  void emergencyStop();

  /**
   * @brief Get current PWM output value
   * @return Signed PWM value last written
   */
  double getOutput() const { return current_output_; }

  /**
   * @brief Get current direction state
   * @return true if forward, false if reverse
   */
  bool isForward() const { return is_forward_; }

  /**
   * @brief Check if motor is effectively stopped
   * @return true if PWM is near zero
   */
  bool isStopped() const { return abs(current_output_) < 0.5; }

private:
  MotorPinConfig config_;
  bool inverted_;
  bool is_forward_;
  double current_output_;

  void setDirection(bool forward);
  void writePWM(uint8_t pwm);
};

#endif // MOTORDRIVER_H