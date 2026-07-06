/**
 * ============================================================================
 * PIDCONTROLLER.H - PID Controller Wrapper with Anti-Windup
 * ============================================================================
 */

#ifndef PIDCONTROLLER_H
#define PIDCONTROLLER_H

#include "Config.h"
#include <Arduino.h>


/**
 * @brief Enhanced PID Controller
 *
 * Wraps the PID_v1 library with additional features:
 * - Automatic output limiting
 * - Anti-windup protection
 * - Enable/disable control
 * - Output smoothing option
 * - Reset capability
 */
class PIDController {
public:
  /**
   * @brief Construct a new PID Controller
   * @param kp Proportional gain
   * @param ki Integral gain
   * @param kd Derivative gain
   * @param output_min Minimum output limit
   * @param output_max Maximum output limit
   * @param sample_time_ms Sample time in milliseconds
   */
  PIDController(double kp, double ki, double kd, double output_min,
                double output_max, int sample_time_ms);

  /**
   * @brief Initialize the PID controller
   */
  void begin();

  /**
   * @brief Compute PID output
   * @param setpoint Desired value
   * @param input Measured value
   * @return Computed output value
   */
  double compute(double setpoint, double input);

  /**
   * @brief Enable or disable the controller
   * @param enabled true to enable, false to disable
   */
  void setEnabled(bool enabled);

  /**
   * @brief Check if controller is enabled
   * @return true if enabled
   */
  bool isEnabled() const { return enabled_; }

  /**
   * @brief Reset controller state (clears integral term)
   */
  void reset();

  /**
   * @brief Update tuning parameters
   * @param kp New proportional gain
   * @param ki New integral gain
   * @param kd New derivative gain
   */
  void setTunings(double kp, double ki, double kd);

  /**
   * @brief Get current output value
   * @return Last computed output
   */
  double getOutput() const { return output_; }

  /**
   * @brief Set output limits
   * @param min Minimum output
   * @param max Maximum output
   */
  void setOutputLimits(double min, double max);

  /**
   * @brief Set sample time
   * @param ms Sample time in milliseconds
   */
  void setSampleTime(int ms);

private:
  double kp_;
  double ki_;
  double kd_;
  double output_min_;
  double output_max_;
  int sample_time_ms_;

  bool enabled_;
  double output_;
  double last_setpoint_;
  double last_input_;

  // Internal PID state for custom anti-windup
  double integral_term_;
  double last_error_;
  bool first_computation_;

  // Core PID computation with anti-windup
  double computeCore(double setpoint, double input, double dt);
};

#endif // PIDCONTROLLER_H