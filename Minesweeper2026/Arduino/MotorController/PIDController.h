/**
 * ============================================================================
 * PIDCONTROLLER.H - PID Controller Wrapper with Anti-Windup & Feedforward
 * ============================================================================
 */

#ifndef PIDCONTROLLER_H
#define PIDCONTROLLER_H

#include "Config.h"
#include <Arduino.h>

class PIDController {
public:
  PIDController(float kp, float ki, float kd, float ff, float output_min,
                float output_max, int sample_time_ms);

  void begin();

  float compute(float setpoint, float input);

  void setEnabled(bool enabled);
  bool isEnabled() const { return enabled_; }

  void reset();

  void setTunings(float kp, float ki, float kd, float ff);
  void setOutputLimits(float min, float max);
  void setSampleTime(int ms);
  
  float getOutput() const { return output_; }
  bool isSaturated() const { return saturated_; }

private:
  float kp_;
  float ki_;
  float kd_;
  float ff_;
  float output_min_;
  float output_max_;
  int sample_time_ms_;

  bool enabled_;
  bool saturated_;
  float output_;
  float last_setpoint_;
  float last_input_;

  // Internal PID state
  float integral_term_;
  float last_error_;
  float last_d_term_;
  bool first_computation_;

  float computeCore(float setpoint, float input, float dt);
};

#endif // PIDCONTROLLER_H