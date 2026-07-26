/**
 * ============================================================================
 * MOTIONCONTROLLER.H - Motion Planning & Trajectory Generation
 * ============================================================================
 */

#ifndef MOTIONCONTROLLER_H
#define MOTIONCONTROLLER_H

#include "Config.h"
#include "EEPROMManager.h"
#include <Arduino.h>

/**
 * @brief Single wheel motion state
 */
struct WheelMotionState {
  float target_velocity;   // Commanded velocity (rad/s)
  float profiled_velocity; // Ramp-limited velocity (rad/s)
  float measured_velocity; // Actual measured velocity (rad/s)
  float target_pwm;        // PID computed output (PWM)
  float current_pwm;       // Slew-rate limited output sent to MotorDriver
};

class MotionController {
public:
  MotionController();
  void begin();

  void setRightTarget(float velocity);
  void setLeftTarget(float velocity);

  /**
   * @brief Update velocity profiles based on max accel/decel
   */
  void updateProfiles(float dt_sec);

  /**
   * @brief Apply deadband compensation and PWM slew rate limiting
   * Takes target_pwm from PID, outputs current_pwm to motors
   */
  void updatePWM(float dt_sec);

  void emergencyStop();

  const WheelMotionState &getRightState() const { return right_state_; }
  const WheelMotionState &getLeftState() const { return left_state_; }

  WheelMotionState &getRightStateMutable() { return right_state_; }
  WheelMotionState &getLeftStateMutable() { return left_state_; }

  bool isStopped() const;
  void reset();

private:
  WheelMotionState right_state_;
  WheelMotionState left_state_;

  float applyVelocityRamping(float target, float current, float dt_sec, const RobotParameters& params) const;
  float applyPWMRamping(float target_pwm, float current_pwm, float dt_sec, const RobotParameters& params) const;
};

#endif // MOTIONCONTROLLER_H