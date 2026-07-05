/**
 * ============================================================================
 * MOTIONCONTROLLER.H - Motion Planning & Trajectory Generation
 * ============================================================================
 */

#ifndef MOTIONCONTROLLER_H
#define MOTIONCONTROLLER_H

#include "Config.h"
#include <Arduino.h>


/**
 * @brief Single wheel motion state
 */
struct WheelMotionState {
  double target_velocity;   // Commanded velocity (rad/s)
  double profiled_velocity; // Ramp-limited velocity (rad/s)
  double measured_velocity; // Actual measured velocity (rad/s)
  double pid_output;        // PID computed output (PWM)
};

/**
 * @brief Motion Controller
 *
 * Handles:
 * - Velocity ramping (acceleration/deceleration limiting)
 * - Deadband compensation
 * - Trajectory profiling
 * - Output conditioning
 */
class MotionController {
public:
  /**
   * @brief Construct a new Motion Controller
   */
  MotionController();

  /**
   * @brief Initialize motion controller
   */
  void begin();

  /**
   * @brief Set target velocity for right wheel
   * @param velocity Target velocity in rad/s (signed)
   */
  void setRightTarget(double velocity);

  /**
   * @brief Set target velocity for left wheel
   * @param velocity Target velocity in rad/s (signed)
   */
  void setLeftTarget(double velocity);

  /**
   * @brief Update velocity profiles (call at control rate)
   * @param dt_sec Time since last update in seconds
   */
  void updateProfiles(double dt_sec);

  /**
   * @brief Apply deadband compensation to PID outputs
   */
  void applyDeadband();

  /**
   * @brief Stop all motion immediately
   */
  void emergencyStop();

  /**
   * @brief Get right wheel motion state
   * @return Const reference to right wheel state
   */
  const WheelMotionState &getRightState() const { return right_state_; }

  /**
   * @brief Get left wheel motion state
   * @return Const reference to left wheel state
   */
  const WheelMotionState &getLeftState() const { return left_state_; }

  /**
   * @brief Get mutable reference to right wheel state (for PID output)
   * @return Reference to right wheel state
   */
  WheelMotionState &getRightStateMutable() { return right_state_; }

  /**
   * @brief Get mutable reference to left wheel state (for PID output)
   * @return Reference to left wheel state
   */
  WheelMotionState &getLeftStateMutable() { return left_state_; }

  /**
   * @brief Check if robot is at rest
   * @return true if both wheels are at zero velocity
   */
  bool isStopped() const;

  /**
   * @brief Reset all motion state
   */
  void reset();

private:
  WheelMotionState right_state_;
  WheelMotionState left_state_;

  double applyRamping(double target, double current, double dt_sec) const;
};

#endif // MOTIONCONTROLLER_H