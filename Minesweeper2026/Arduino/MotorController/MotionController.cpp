/**
 * ============================================================================
 * MOTIONCONTROLLER.CPP - Motion Controller Implementation
 * ============================================================================
 */

#include "MotionController.h"

MotionController::MotionController() { reset(); }

void MotionController::begin() { reset(); }

void MotionController::setRightTarget(double velocity) {
  // Sanity check
  if (abs(velocity) > Safety::MAX_VELOCITY_RAD_S) {
    velocity = (velocity > 0) ? Safety::MAX_VELOCITY_RAD_S
                              : -Safety::MAX_VELOCITY_RAD_S;
  }
  right_state_.target_velocity = velocity;
}

void MotionController::setLeftTarget(double velocity) {
  // Sanity check
  if (abs(velocity) > Safety::MAX_VELOCITY_RAD_S) {
    velocity = (velocity > 0) ? Safety::MAX_VELOCITY_RAD_S
                              : -Safety::MAX_VELOCITY_RAD_S;
  }
  left_state_.target_velocity = velocity;
}

void MotionController::updateProfiles(double dt_sec) {
  if (dt_sec <= 0.0)
    dt_sec = Timing::CONTROL_INTERVAL_S;

  right_state_.profiled_velocity = applyRamping(
      right_state_.target_velocity, right_state_.profiled_velocity, dt_sec);

  left_state_.profiled_velocity = applyRamping(
      left_state_.target_velocity, left_state_.profiled_velocity, dt_sec);
}

double MotionController::applyRamping(double target, double current,
                                      double dt_sec) const {
  double error = target - current;
  double max_step_accel = MotionLimits::MAX_ACCELERATION * dt_sec;
  double max_step_decel = MotionLimits::MAX_DECELERATION * dt_sec;

  // Determine appropriate limit based on direction
  double max_step;
  if ((error > 0 && target > current) || (error < 0 && target < current)) {
    // Accelerating toward target
    max_step = max_step_accel;
  } else {
    // Decelerating (changing direction or slowing down)
    max_step = max_step_decel;
  }

  if (error > max_step) {
    return current + max_step;
  } else if (error < -max_step) {
    return current - max_step;
  } else {
    return target;
  }
}

void MotionController::applyDeadband() {
  // Right wheel deadband
  if (abs(right_state_.profiled_velocity) < MotionLimits::VELOCITY_TOLERANCE) {
    right_state_.pid_output = 0.0;
  } else if (abs(right_state_.pid_output) < MotionLimits::MIN_OUTPUT_DEADBAND) {
    right_state_.pid_output = (right_state_.pid_output >= 0)
                                  ? MotionLimits::MIN_OUTPUT_DEADBAND
                                  : -MotionLimits::MIN_OUTPUT_DEADBAND;
  }

  // Left wheel deadband
  if (abs(left_state_.profiled_velocity) < MotionLimits::VELOCITY_TOLERANCE) {
    left_state_.pid_output = 0.0;
  } else if (abs(left_state_.pid_output) < MotionLimits::MIN_OUTPUT_DEADBAND) {
    left_state_.pid_output = (left_state_.pid_output >= 0)
                                 ? MotionLimits::MIN_OUTPUT_DEADBAND
                                 : -MotionLimits::MIN_OUTPUT_DEADBAND;
  }
}

void MotionController::emergencyStop() {
  right_state_.target_velocity = 0.0;
  left_state_.target_velocity = 0.0;
  right_state_.profiled_velocity = 0.0;
  left_state_.profiled_velocity = 0.0;
  right_state_.pid_output = 0.0;
  left_state_.pid_output = 0.0;
}

bool MotionController::isStopped() const {
  return abs(right_state_.profiled_velocity) <
             MotionLimits::VELOCITY_TOLERANCE &&
         abs(left_state_.profiled_velocity) < MotionLimits::VELOCITY_TOLERANCE;
}

void MotionController::reset() {
  right_state_.target_velocity = 0.0;
  right_state_.profiled_velocity = 0.0;
  right_state_.measured_velocity = 0.0;
  right_state_.pid_output = 0.0;

  left_state_.target_velocity = 0.0;
  left_state_.profiled_velocity = 0.0;
  left_state_.measured_velocity = 0.0;
  left_state_.pid_output = 0.0;
}