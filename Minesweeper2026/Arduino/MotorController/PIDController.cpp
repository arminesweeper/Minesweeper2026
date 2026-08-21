/**
 * ============================================================================
 * PIDCONTROLLER.CPP - PID Controller Implementation
 * ============================================================================
 */

#include "PIDController.h"

PIDController::PIDController(float kp, float ki, float kd, float ff, float output_min,
                             float output_max, int sample_time_ms)
    : kp_(kp), ki_(ki), kd_(kd), ff_(ff), output_min_(output_min),
      output_max_(output_max), sample_time_ms_(sample_time_ms), enabled_(false), saturated_(false),
      output_(0.0f), last_setpoint_(0.0f), last_input_(0.0f), integral_term_(0.0f),
      last_error_(0.0f), last_d_term_(0.0f), first_computation_(true) {}

void PIDController::begin() {
  reset();
  enabled_ = true;
}

float PIDController::compute(float setpoint, float input) {
  if (!enabled_) {
    return output_;
  }

  float dt = sample_time_ms_ / 1000.0f;
  output_ = computeCore(setpoint, input, dt);

  return output_;
}

float PIDController::computeCore(float setpoint, float input, float dt) {
  // Calculate error
  float error = setpoint - input;

  // Proportional term
  float p_term = kp_ * error;

  // Integral term
  integral_term_ += ki_ * error * dt;

  // Clamp integral term to prevent windup
  // Note: Dynamic clamping happens at the end to prevent saturation windup,
  // but we also apply a hard absolute limit to avoid numerical overflow over long periods.
  float integral_limit = 200.0f; 
  if (integral_term_ > integral_limit) {
    integral_term_ = integral_limit;
  } else if (integral_term_ < -integral_limit) {
    integral_term_ = -integral_limit;
  }

  // Derivative term (on error) with Low-Pass Filtering to reduce noise
  float d_term = 0.0f;
  if (!first_computation_ && dt > 0.0f) {
    float raw_d_term = kd_ * (error - last_error_) / dt;
    // Low pass filter alpha ~ 0.1 for smoothing out encoder jitter
    float alpha = 0.1f; 
    d_term = alpha * raw_d_term + (1.0f - alpha) * last_d_term_;
  }
  first_computation_ = false;
  last_error_ = error;
  last_d_term_ = d_term;

  // Feedforward term
  float ff_term = ff_ * setpoint;

  // Calculate total output
  float output = p_term + integral_term_ + d_term + ff_term;

  // Apply output limits and saturation flag
  saturated_ = false;
  if (output > output_max_) {
    output = output_max_;
    saturated_ = true;
    // Back-calculate integral to prevent windup at saturation
    integral_term_ = output - p_term - d_term - ff_term;
  } else if (output < output_min_) {
    output = output_min_;
    saturated_ = true;
    // Back-calculate integral to prevent windup at saturation
    integral_term_ = output - p_term - d_term - ff_term;
  }

  last_setpoint_ = setpoint;
  last_input_ = input;

  return output;
}

void PIDController::setEnabled(bool enabled) {
  if (!enabled && enabled_) {
    output_ = 0.0f;
  }
  enabled_ = enabled;
}

void PIDController::reset() {
  integral_term_ = 0.0f;
  last_error_ = 0.0f;
  last_d_term_ = 0.0f;
  output_ = 0.0f;
  first_computation_ = true;
  saturated_ = false;
}

void PIDController::setTunings(float kp, float ki, float kd, float ff) {
  kp_ = kp;
  ki_ = ki;
  kd_ = kd;
  ff_ = ff;
}

void PIDController::setOutputLimits(float min, float max) {
  output_min_ = min;
  output_max_ = max;
}

void PIDController::setSampleTime(int ms) { sample_time_ms_ = ms; }