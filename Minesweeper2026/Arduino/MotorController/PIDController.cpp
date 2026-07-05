/**
 * ============================================================================
 * PIDCONTROLLER.CPP - PID Controller Implementation
 * ============================================================================
 */

#include "PIDController.h"

PIDController::PIDController(double kp, double ki, double kd, double output_min,
                             double output_max, int sample_time_ms)
    : kp_(kp), ki_(ki), kd_(kd), output_min_(output_min),
      output_max_(output_max), sample_time_ms_(sample_time_ms), enabled_(false),
      output_(0.0), last_setpoint_(0.0), last_input_(0.0), integral_term_(0.0),
      last_error_(0.0), first_computation_(true) {}

void PIDController::begin() {
  reset();
  enabled_ = true;
}

double PIDController::compute(double setpoint, double input) {
  if (!enabled_) {
    return output_;
  }

  double dt = sample_time_ms_ / 1000.0;
  output_ = computeCore(setpoint, input, dt);

  return output_;
}

double PIDController::computeCore(double setpoint, double input, double dt) {
  // Calculate error
  double error = setpoint - input;

  // Proportional term
  double p_term = kp_ * error;

  // Integral term with anti-windup
  integral_term_ += ki_ * error * dt;

  // Clamp integral term to prevent windup
  double integral_limit = PIDTuning::INTEGRAL_WINDUP_LIMIT;
  if (integral_term_ > integral_limit) {
    integral_term_ = integral_limit;
  } else if (integral_term_ < -integral_limit) {
    integral_term_ = -integral_limit;
  }

  // Derivative term (on error, with filtering to reduce noise)
  double d_term = 0.0;
  if (!first_computation_) {
    d_term = kd_ * (error - last_error_) / dt;
  }
  first_computation_ = false;
  last_error_ = error;

  // Calculate total output
  double output = p_term + integral_term_ + d_term;

  // Apply output limits
  if (output > output_max_) {
    output = output_max_;
    // Back-calculate integral to prevent windup at saturation
    integral_term_ = output - p_term - d_term;
  } else if (output < output_min_) {
    output = output_min_;
    // Back-calculate integral to prevent windup at saturation
    integral_term_ = output - p_term - d_term;
  }

  last_setpoint_ = setpoint;
  last_input_ = input;

  return output;
}

void PIDController::setEnabled(bool enabled) {
  if (!enabled && enabled_) {
    // Transitioning from enabled to disabled - reset state
    output_ = 0.0;
  }
  enabled_ = enabled;
}

void PIDController::reset() {
  integral_term_ = 0.0;
  last_error_ = 0.0;
  output_ = 0.0;
  first_computation_ = true;
}

void PIDController::setTunings(double kp, double ki, double kd) {
  kp_ = kp;
  ki_ = ki;
  kd_ = kd;
}

void PIDController::setOutputLimits(double min, double max) {
  output_min_ = min;
  output_max_ = max;
}

void PIDController::setSampleTime(int ms) { sample_time_ms_ = ms; }