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
  explicit MotorDriver(const MotorPinConfig &config, bool inverted = false);

  void begin();

  /**
   * @brief Set motor output with signed PWM value
   * @param pwm Signed PWM value (-255.0 to +255.0)
   */
  void setOutput(float pwm);

  /**
   * @brief Immediately stop the motor
   */
  void stop();

  /**
   * @brief Emergency stop - disables output completely
   */
  void emergencyStop();

  float getOutput() const { return current_output_; }
  bool isForward() const { return is_forward_; }
  bool isStopped() const { return abs(current_output_) < 0.5f; }

  void setInverted(bool inverted) { inverted_ = inverted; }

private:
  MotorPinConfig config_;
  bool inverted_;
  bool is_forward_;
  float current_output_;

  void setDirection(bool forward);
  void writePWM(uint8_t pwm);
};

#endif // MOTORDRIVER_H