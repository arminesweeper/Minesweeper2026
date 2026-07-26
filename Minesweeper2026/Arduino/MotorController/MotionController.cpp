/**
 * ============================================================================
 * MOTIONCONTROLLER.CPP - Motion Controller Implementation
 * ============================================================================
 */

#include "MotionController.h"

MotionController::MotionController() { reset(); }

void MotionController::begin() { reset(); }

void MotionController::setRightTarget(float velocity) {
  const RobotParameters& params = EEPROMManager::getParams();
  if (abs(velocity) > params.max_velocity) {
    velocity = (velocity > 0.0f) ? params.max_velocity : -params.max_velocity;
  }
  right_state_.target_velocity = velocity;
}

void MotionController::setLeftTarget(float velocity) {
  const RobotParameters& params = EEPROMManager::getParams();
  if (abs(velocity) > params.max_velocity) {
    velocity = (velocity > 0.0f) ? params.max_velocity : -params.max_velocity;
  }
  left_state_.target_velocity = velocity;
}

void MotionController::updateProfiles(float dt_sec) {
  if (dt_sec <= 0.0f) return;
  
  const RobotParameters& params = EEPROMManager::getParams();

  right_state_.profiled_velocity = applyVelocityRamping(
      right_state_.target_velocity, right_state_.profiled_velocity, dt_sec, params);

  left_state_.profiled_velocity = applyVelocityRamping(
      left_state_.target_velocity, left_state_.profiled_velocity, dt_sec, params);
}

float MotionController::applyVelocityRamping(float target, float current, float dt_sec, const RobotParameters& params) const {
  float error = target - current;
  float max_step_accel = params.max_acceleration * dt_sec;
  float max_step_decel = params.max_deceleration * dt_sec;

  float max_step;
  if ((error > 0.0f && target > current) || (error < 0.0f && target < current)) {
    max_step = max_step_accel;
  } else {
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

void MotionController::updatePWM(float dt_sec) {
  if (dt_sec <= 0.0f) return;

  const RobotParameters& params = EEPROMManager::getParams();
  
  // Apply deadband
  float target_pwm_r = right_state_.target_pwm;
  if (abs(right_state_.profiled_velocity) < 0.01f) {
    target_pwm_r = 0.0f;
  } else if (abs(target_pwm_r) > 0.0f && abs(target_pwm_r) < params.min_pwm_deadband) {
    target_pwm_r = (target_pwm_r >= 0.0f) ? params.min_pwm_deadband : -params.min_pwm_deadband;
  }

  float target_pwm_l = left_state_.target_pwm;
  if (abs(left_state_.profiled_velocity) < 0.01f) {
    target_pwm_l = 0.0f;
  } else if (abs(target_pwm_l) > 0.0f && abs(target_pwm_l) < params.min_pwm_deadband) {
    target_pwm_l = (target_pwm_l >= 0.0f) ? params.min_pwm_deadband : -params.min_pwm_deadband;
  }

  // Apply PWM Slew Rate Limiting
  right_state_.current_pwm = applyPWMRamping(target_pwm_r, right_state_.current_pwm, dt_sec, params);
  left_state_.current_pwm = applyPWMRamping(target_pwm_l, left_state_.current_pwm, dt_sec, params);
}

float MotionController::applyPWMRamping(float target_pwm, float current_pwm, float dt_sec, const RobotParameters& params) const {
  float max_step = params.max_pwm_slew_rate * dt_sec;
  float error = target_pwm - current_pwm;
  
  if (error > max_step) {
    return current_pwm + max_step;
  } else if (error < -max_step) {
    return current_pwm - max_step;
  } else {
    return target_pwm;
  }
}

void MotionController::emergencyStop() {
  right_state_.target_velocity = 0.0f;
  left_state_.target_velocity = 0.0f;
  right_state_.profiled_velocity = 0.0f;
  left_state_.profiled_velocity = 0.0f;
  right_state_.target_pwm = 0.0f;
  left_state_.target_pwm = 0.0f;
  right_state_.current_pwm = 0.0f;
  left_state_.current_pwm = 0.0f;
}

bool MotionController::isStopped() const {
  return abs(right_state_.profiled_velocity) < 0.01f &&
         abs(left_state_.profiled_velocity) < 0.01f;
}

void MotionController::reset() {
  right_state_.target_velocity = 0.0f;
  right_state_.profiled_velocity = 0.0f;
  right_state_.measured_velocity = 0.0f;
  right_state_.target_pwm = 0.0f;
  right_state_.current_pwm = 0.0f;

  left_state_.target_velocity = 0.0f;
  left_state_.profiled_velocity = 0.0f;
  left_state_.measured_velocity = 0.0f;
  left_state_.target_pwm = 0.0f;
  left_state_.current_pwm = 0.0f;
}