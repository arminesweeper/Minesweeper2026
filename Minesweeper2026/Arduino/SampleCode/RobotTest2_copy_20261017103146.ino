/*
 * Minesweeper Rover - ROS 2 compatible Arduino Mega 2560 firmware
 *
 * Built by combining the known-good single-file motor/encoder test sketch
 * with the sensor/siren pin information from the FreeRTOS reference firmware.
 *
 * ROS2 roverInterface RX (115200 baud):
 *   rp1.00,lp1.00,gp0.00,sp0.00,

 *   rn0.50,ln0.50,gp0.00,sp0.00,

 *   CSERVO:90

 *
 * Telemetry TX (one line, repeatedly):
 *   rp1.82,lp1.79,p0,m0,q1:123,q2:456,q3:234,q4:400,q5:100,er:1234,el:-1234,

 *   The current roverInterface consumes rp/rn, lp/ln, p0/p1 and m0/m1.
 *   Extra q/er/el tokens are diagnostic and are ignored by that interface.
 *   The ROS hardware interface also parses er/el to populate encoder joint positions.
 * Extended tokens: cycle:1, grip:reset, plus additive telemetry us/uv/cs/co/mg/si.
 *
 * IMPORTANT WIRING:
 *   Motor/encoder/camera pins follow the tested single-file sketch.
 *   Proximity D25/D23/D12/D13, metal D27, siren D47, lift D7/D49 and
 *   limit switches D51/D53 follow the supplied original FreeRTOS firmware.
 *   The separate buzzer pin was NOT specified in either source, so that output
 *   stays disabled. Set BUZZER_PIN only after confirming the physical wiring.
 *
 * Safety: test with drive wheels lifted off the floor and a reachable battery
 * disconnect. Do not power a siren/buzzer directly from an Arduino GPIO; use
 * the required transistor/relay/driver and a suitable external supply.
 */

#include <Arduino.h>
#include <util/atomic.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

// -------------------- Pins: from the GitHub firmware --------------------
constexpr uint8_t MOTOR_R_PWM   = 44;
constexpr uint8_t MOTOR_R_DIR   = 42;
constexpr uint8_t MOTOR_L_PWM   = 46;
constexpr uint8_t MOTOR_L_DIR   = 40;
constexpr uint8_t MOTOR_R_PWM_2 = 9;
constexpr uint8_t MOTOR_R_DIR_2 = 8;
constexpr uint8_t MOTOR_L_PWM_2 = 11;
constexpr uint8_t MOTOR_L_DIR_2 = 10;

constexpr uint8_t ENCODER_R_A = 19;
constexpr uint8_t ENCODER_R_B = 18;
constexpr uint8_t ENCODER_L_A = 21;
constexpr uint8_t ENCODER_L_B = 20;

constexpr uint8_t CAMERA_SERVO_PIN = 4;

// Same motor/encoder inversion used by the referenced firmware.
constexpr bool RIGHT_MOTOR_INVERTED = false;
constexpr bool LEFT_MOTOR_INVERTED  = true;
constexpr bool RIGHT_ENCODER_INVERTED = false;
constexpr bool LEFT_ENCODER_INVERTED  = true;

// -------------------- Control --------------------
constexpr float ENCODER_PPR = 2200.0f;
constexpr float RAD_PER_PULSE = (2.0f * PI) / ENCODER_PPR;
constexpr uint32_t CONTROL_PERIOD_MS = 20;       // 50 Hz
constexpr uint32_t TELEMETRY_PERIOD_MS = 10;     // 100 Hz, match ros2_control read loop
constexpr uint32_t SENSOR_PERIOD_MS = 20;        // 50 Hz
constexpr uint32_t COMMAND_TIMEOUT_MS = 400;
constexpr float MAX_WHEEL_VELOCITY = 6.0f;

constexpr float MAX_ACCELERATION = 30.0f;
constexpr float MAX_DECELERATION = 20.0f;

// Starting PID values from the referenced firmware.
constexpr float KP_RIGHT = 11.5f;
constexpr float KI_RIGHT = 7.5f;
constexpr float KD_RIGHT = 0.1f;
constexpr float FF_RIGHT = 0.5f;
constexpr float KP_LEFT  = 12.8f;
constexpr float KI_LEFT  = 8.3f;
constexpr float KD_LEFT  = 0.1f;
constexpr float FF_LEFT  = 0.5f;

constexpr float PID_OUTPUT_MIN = -255.0f;
constexpr float PID_OUTPUT_MAX = 255.0f;
constexpr float INTEGRAL_LIMIT = 200.0f;
constexpr float MIN_PWM_DEADBAND = 20.0f;
constexpr float PWM_SLEW_RATE = 1000.0f;

// -------------------- Sensors / siren / lift-gripper --------------------
// Sensor pins below follow the supplied original FreeRTOS firmware.
constexpr uint8_t PROXIMITY_COUNT = 3;
constexpr uint8_t PROXIMITY_PINS[PROXIMITY_COUNT] = {25, 23, 12};
constexpr uint8_t PROXIMITY_ACTIVE_STATE = HIGH;  // Optical proximity sensors are active-high.
constexpr uint8_t METAL_DETECTOR_PIN = 27;
constexpr uint8_t METAL_DETECTOR_PIN_2 = 33;      // PCB shield metal detector header
constexpr uint8_t METAL_ACTIVE_STATE = HIGH;      // Confirmed in original FreeRTOS firmware.
constexpr uint32_t METAL_DEBOUNCE_MS = 50;
constexpr uint8_t PROXIMITY_DEBOUNCE_SAMPLES = 2;  // updateSensors runs every 20 ms.

constexpr uint8_t SIREN_PIN = 47;
constexpr uint8_t SIREN_PIN_SHIELD = 37;          // PCB shield siren relay driver
constexpr bool SIREN_ACTIVE_HIGH = true;
constexpr uint8_t MAGNET_PIN = 38;                // Starts disabled; no ROS command wired here.

// The physical lift/gripper pins come from the original FreeRTOS firmware.
constexpr uint8_t LIFT_PWM = 7;
constexpr uint8_t LIFT_DIR = 49;
constexpr uint8_t LIMIT_SW_TOP = 51;
constexpr uint8_t LIMIT_SW_BOTTOM = 53;
// Original code used INPUT and noted external pull-downs. HIGH is therefore
// treated as an asserted limit. If your switches are wired active-low, change this.
constexpr uint8_t LIMIT_SWITCH_ACTIVE_STATE = HIGH;
constexpr bool TOP_LIMIT_MEANS_CLOSED = true;
// This is the only hardware-direction assumption: confirm on a no-load test.
constexpr bool LIFT_CLOSE_DIR_HIGH = true;
constexpr uint8_t LIFT_PWM_PERCENT = 80;
constexpr uint32_t GRIPPER_COMMAND_TIMEOUT_MS = 500;
constexpr float MAX_GRIPPER_COMMAND = 3.0f;

// -------------------- Ultrasonic sensing / pickup cycle --------------------
// D43/D45 are borrowed from the earlier reference firmware. VERIFY the physical
// wiring before enabling the sensor or powering the lift.
constexpr uint8_t US_TRIG_PIN = 43;
constexpr uint8_t US_ECHO_PIN = 45;
constexpr uint8_t US_AVG_N = 5;
constexpr uint32_t US_SAMPLE_INTERVAL_MS = 60;
constexpr uint32_t US_ECHO_TIMEOUT_US = 30000UL;
constexpr float US_MAX_DISTANCE_CM = 200.0f;
constexpr float US_OBJECT_CHANGE_CM = 3.0f;
constexpr uint8_t US_OBJECT_PERSISTENCE = 3;
constexpr uint8_t US_BASELINE_SAMPLES = 5;
// Automatic pickup is armed only after a valid empty baseline and additionally
// gated by an active proximity/metal sensor, reducing terrain-change false trips.
constexpr bool ULTRASONIC_AUTO_CYCLE_ENABLED = false;  // Enable only after bench calibration.
constexpr bool US_REQUIRE_DIGITAL_DETECTION_FOR_AUTO_CYCLE = false;
// The geometry has not been measured yet. Keep false until release-vs-held
// readings are measured with the actual gripper/mine/box; then test and enable.
constexpr bool ULTRASONIC_RELEASE_VERIFY_ENABLED = false;
constexpr float US_RELEASE_CLEAR_DISTANCE_CM = 20.0f;

// Full-cycle lift parameters are intentionally separate from wheel PID settings.
constexpr uint8_t CYCLE_LIFT_PWM_PERCENT = 75;
constexpr uint32_t MAGNET_SETTLE_MS = 200;
constexpr uint32_t RELEASE_DWELL_MS = 750;
constexpr uint32_t RELEASE_VERIFY_TIMEOUT_MS = 2500;
constexpr uint32_t MAX_LIFT_TRAVEL_MS = 10000;
constexpr uint32_t SIREN_HOLD_MS = 1500;
constexpr bool MAGNET_ALWAYS_ON_WHEN_DOWN = true;

// The uploaded roverInterface's gripper_joy_teleop logs use OPEN=0 and CLOSE=-2.967.
// Therefore negative g velocity moves toward CLOSED, positive moves toward OPEN.
// Keep BUZZER_PIN disabled until its actual wired pin is known; D47 is the siren.
constexpr int16_t BUZZER_PIN = -1;
constexpr bool BUZZER_ACTIVE_HIGH = true;
constexpr uint32_t BUZZER_TOGGLE_MS = 200;

// -------------------- Camera --------------------
constexpr int CAMERA_MIN_ANGLE = 10;
constexpr int CAMERA_MAX_ANGLE = 170;
constexpr int CAMERA_CENTER_ANGLE = 90;
constexpr int CAMERA_STEP_DEG = 2;
constexpr uint32_t CAMERA_COMMAND_TIMEOUT_MS = 300;
constexpr uint32_t SERVO_FRAME_US = 20000;
constexpr uint32_t SERVO_MIN_PULSE_US = 1000;
constexpr uint32_t SERVO_MAX_PULSE_US = 2000;

struct WheelState {
  float target_velocity = 0.0f;
  float profiled_velocity = 0.0f;
  float measured_velocity = 0.0f;
  float integral_term = 0.0f;
  float last_error = 0.0f;
  float last_d_term = 0.0f;
  float target_pwm = 0.0f;
  float current_pwm = 0.0f;
};

WheelState right_wheel, left_wheel;

volatile int32_t right_pulse_count = 0;  // delta pulses used for velocity
volatile int32_t left_pulse_count = 0;
volatile int32_t right_total_pulses = 0;  // cumulative encoder counts for diagnostics
volatile int32_t left_total_pulses = 0;

uint32_t last_control_ms = 0;
uint32_t last_telemetry_ms = 0;
uint32_t last_sensor_ms = 0;
uint32_t last_command_ms = 0;

uint8_t proximity_values[PROXIMITY_COUNT] = {0};
bool proximity_triggered = false;
bool proximity_candidate_state = false;
uint8_t proximity_candidate_samples = 0;
bool metal_detected = false;
bool metal_last_raw = false;
uint32_t metal_last_change_ms = 0;

float gripper_target_velocity = 0.0f;
uint32_t last_gripper_command_ms = 0;
char gripper_status = 'm';  // 'o' open, 'c' closed, 'm' between limits/moving
bool gripper_is_moving = false;
uint32_t last_buzzer_toggle_ms = 0;

// Lift/gripper state machine. The wheel control loop and its PID are unchanged.
enum GripperState : uint8_t {
  GRIP_BOOT = 0,
  GRIP_DOWN_READY,
  GRIP_MAGNET_SETTLE,
  GRIP_MOVING_UP,
  GRIP_TOP_RELEASE,
  GRIP_MOVING_DOWN,
  GRIP_RECOVERY_DOWN,
  GRIP_FAULT
};
GripperState grip_state = GRIP_BOOT;
uint32_t grip_state_started_ms = 0;
uint32_t manual_lift_started_ms = 0;
bool manual_lift_timing = false;
bool manual_lift_last_closing = false;
bool magnet_state = false;
bool siren_state = false;
uint32_t siren_hold_until_ms = 0;
bool prev_proximity_for_edge = false;
bool prev_metal_for_edge = false;
bool auto_cycle_latched = false;
bool cycle_release_verified = false;
char cycle_outcome[24] = "NONE";
char grip_fault_reason[24] = "NONE";

// Non-blocking HC-SR04 state and rolling filter.
enum UltrasonicState : uint8_t { US_IDLE = 0, US_TRIGGER_HIGH, US_WAIT_RISE, US_WAIT_FALL };
UltrasonicState us_state = US_IDLE;
uint32_t us_state_started_us = 0;
uint32_t us_echo_rise_us = 0;
uint32_t last_us_sample_ms = 0;
uint32_t us_sample_seq = 0;
uint32_t us_last_valid_ms = 0;
bool us_last_reading_valid = false;
float us_last_distance_cm = 0.0f;
float us_filtered_distance_cm = 0.0f;
float us_readings[US_AVG_N] = {0};
uint8_t us_reading_count = 0;
uint8_t us_reading_next = 0;
bool us_baseline_ready = false;
float us_baseline_cm = 0.0f;
float us_baseline_accum = 0.0f;
uint8_t us_baseline_sample_count = 0;
uint8_t us_object_persist_count = 0;
uint8_t us_absent_persist_count = 0;
bool us_object_present = false;
uint32_t us_last_baseline_seq = 0;
uint32_t us_last_object_seq = 0;
uint32_t us_last_release_seq = 0;
uint8_t us_release_clear_count = 0;
bool buzzer_output_on = false;

int camera_angle = CAMERA_CENTER_ANGLE;
int camera_direction = 0;
uint32_t camera_last_command_ms = 0;

bool servo_pulse_high = false;
uint32_t servo_frame_start_us = 0;
uint32_t servo_pulse_width_us = 1500;
uint32_t last_camera_step_ms = 0;

// -------------------- Helpers --------------------
float clampFloat(float v, float lo, float hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

int clampInt(int v, int lo, int hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

// -------------------- Motors --------------------
void setMotorOutput(uint8_t pwm_pin, uint8_t dir_pin, float pwm, bool inverted) {
  pwm = clampFloat(pwm, -255.0f, 255.0f);
  bool forward = (pwm >= 0.0f);
  if (inverted) forward = !forward;
  digitalWrite(dir_pin, forward ? HIGH : LOW);
  analogWrite(pwm_pin, static_cast<uint8_t>(fabs(pwm)));
}

void stopAllMotors() {
  analogWrite(MOTOR_R_PWM, 0);
  analogWrite(MOTOR_L_PWM, 0);
  analogWrite(MOTOR_R_PWM_2, 0);
  analogWrite(MOTOR_L_PWM_2, 0);
  right_wheel.current_pwm = 0.0f;
  left_wheel.current_pwm = 0.0f;
}

void applyDrivePWM() {
  setMotorOutput(MOTOR_R_PWM,   MOTOR_R_DIR,   right_wheel.current_pwm, RIGHT_MOTOR_INVERTED);
  setMotorOutput(MOTOR_R_PWM_2, MOTOR_R_DIR_2, right_wheel.current_pwm, RIGHT_MOTOR_INVERTED);
  setMotorOutput(MOTOR_L_PWM,   MOTOR_L_DIR,   left_wheel.current_pwm, LEFT_MOTOR_INVERTED);
  setMotorOutput(MOTOR_L_PWM_2, MOTOR_L_DIR_2, left_wheel.current_pwm, LEFT_MOTOR_INVERTED);
}

// -------------------- Encoders --------------------
void rightEncoderISR() {
  const int8_t delta = digitalRead(ENCODER_R_B) ? 1 : -1;
  right_pulse_count += delta;
  right_total_pulses += delta;
}

void leftEncoderISR() {
  const int8_t delta = digitalRead(ENCODER_L_B) ? 1 : -1;
  left_pulse_count += delta;
  left_total_pulses += delta;
}

int32_t takeRightPulses() {
  int32_t p;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { p = right_pulse_count; right_pulse_count = 0; }
  return p;
}

int32_t takeLeftPulses() {
  int32_t p;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { p = left_pulse_count; left_pulse_count = 0; }
  return p;
}

float calculateWheelVelocity(int32_t delta_pulses, float dt, bool inverted) {
  if (dt <= 0.0f) return 0.0f;
  float v = (static_cast<float>(delta_pulses) * RAD_PER_PULSE) / dt;
  return inverted ? -v : v;
}

// -------------------- Motion + PID --------------------
float applyVelocityRamp(float target, float current, float dt) {
  float error = target - current;
  float accel_step = MAX_ACCELERATION * dt;
  float decel_step = MAX_DECELERATION * dt;
  float max_step = ((error > 0.0f && target > current) || (error < 0.0f && target < current))
                     ? accel_step : decel_step;
  if (error > max_step) return current + max_step;
  if (error < -max_step) return current - max_step;
  return target;
}

float computePID(WheelState &w, float kp, float ki, float kd, float ff, float dt) {
  const float error = w.profiled_velocity - w.measured_velocity;
  const float p_term = kp * error;

  w.integral_term += ki * error * dt;
  w.integral_term = clampFloat(w.integral_term, -INTEGRAL_LIMIT, INTEGRAL_LIMIT);

  float d_term = 0.0f;
  if (dt > 0.0f) {
    const float raw_d = kd * (error - w.last_error) / dt;
    constexpr float alpha = 0.1f;
    d_term = alpha * raw_d + (1.0f - alpha) * w.last_d_term;
  }
  w.last_error = error;
  w.last_d_term = d_term;

  const float ff_term = ff * w.profiled_velocity;
  float output = p_term + w.integral_term + d_term + ff_term;

  if (output > PID_OUTPUT_MAX) {
    output = PID_OUTPUT_MAX;
    w.integral_term = output - p_term - d_term - ff_term;
  } else if (output < PID_OUTPUT_MIN) {
    output = PID_OUTPUT_MIN;
    w.integral_term = output - p_term - d_term - ff_term;
  }
  return output;
}

float applyPWMRamp(float target_pwm, float current_pwm, float dt) {
  const float max_step = PWM_SLEW_RATE * dt;
  const float error = target_pwm - current_pwm;
  if (error > max_step) return current_pwm + max_step;
  if (error < -max_step) return current_pwm - max_step;
  return target_pwm;
}

void updateControl() {
  const float dt = CONTROL_PERIOD_MS / 1000.0f;

  if (millis() - last_command_ms > COMMAND_TIMEOUT_MS) {
    right_wheel.target_velocity = 0.0f;
    left_wheel.target_velocity = 0.0f;
  }

  right_wheel.measured_velocity = calculateWheelVelocity(takeRightPulses(), dt, RIGHT_ENCODER_INVERTED);
  left_wheel.measured_velocity = calculateWheelVelocity(takeLeftPulses(), dt, LEFT_ENCODER_INVERTED);

  right_wheel.profiled_velocity = applyVelocityRamp(right_wheel.target_velocity, right_wheel.profiled_velocity, dt);
  left_wheel.profiled_velocity = applyVelocityRamp(left_wheel.target_velocity, left_wheel.profiled_velocity, dt);

  right_wheel.target_pwm = computePID(right_wheel, KP_RIGHT, KI_RIGHT, KD_RIGHT, FF_RIGHT, dt);
  left_wheel.target_pwm = computePID(left_wheel, KP_LEFT, KI_LEFT, KD_LEFT, FF_LEFT, dt);

  if (fabs(right_wheel.profiled_velocity) < 0.01f) right_wheel.target_pwm = 0.0f;
  else if (fabs(right_wheel.target_pwm) > 0.0f && fabs(right_wheel.target_pwm) < MIN_PWM_DEADBAND)
    right_wheel.target_pwm = (right_wheel.target_pwm >= 0.0f) ? MIN_PWM_DEADBAND : -MIN_PWM_DEADBAND;

  if (fabs(left_wheel.profiled_velocity) < 0.01f) left_wheel.target_pwm = 0.0f;
  else if (fabs(left_wheel.target_pwm) > 0.0f && fabs(left_wheel.target_pwm) < MIN_PWM_DEADBAND)
    left_wheel.target_pwm = (left_wheel.target_pwm >= 0.0f) ? MIN_PWM_DEADBAND : -MIN_PWM_DEADBAND;

  right_wheel.current_pwm = applyPWMRamp(right_wheel.target_pwm, right_wheel.current_pwm, dt);
  left_wheel.current_pwm = applyPWMRamp(left_wheel.target_pwm, left_wheel.current_pwm, dt);

  applyDrivePWM();
}

// -------------------- Sensors --------------------
void updateSensors() {
  bool raw_proximity = false;
  for (uint8_t i = 0; i < PROXIMITY_COUNT; ++i) {
    const uint8_t raw = digitalRead(PROXIMITY_PINS[i]);
    proximity_values[i] = raw;  // q1..q4 remain raw diagnostic values.
    if (raw == PROXIMITY_ACTIVE_STATE) raw_proximity = true;
  }

  // Debounce the aggregate proximity level over consecutive 20 ms samples.
  // Individual q tokens remain raw, while events, siren and map edges use this
  // stable state to avoid chatter on a noisy sensor line.
  if (raw_proximity == proximity_triggered) {
    proximity_candidate_state = raw_proximity;
    proximity_candidate_samples = 0;
  } else if (raw_proximity != proximity_candidate_state) {
    proximity_candidate_state = raw_proximity;
    proximity_candidate_samples = 1;
  } else if (proximity_candidate_samples < PROXIMITY_DEBOUNCE_SAMPLES) {
    ++proximity_candidate_samples;
  }
  if (proximity_candidate_samples >= PROXIMITY_DEBOUNCE_SAMPLES) {
    proximity_triggered = proximity_candidate_state;
    proximity_candidate_samples = 0;
  }

  const bool raw_metal = (digitalRead(METAL_DETECTOR_PIN) == METAL_ACTIVE_STATE ||
                          digitalRead(METAL_DETECTOR_PIN_2) == METAL_ACTIVE_STATE);
  if (raw_metal != metal_last_raw) {
    metal_last_raw = raw_metal;
    metal_last_change_ms = millis();
  }
  if (millis() - metal_last_change_ms >= METAL_DEBOUNCE_MS) {
    metal_detected = metal_last_raw;
  }
}

void updateAlarmOutputs() {
  const uint32_t now = millis();
  const bool prox_rise = proximity_triggered && !prev_proximity_for_edge;
  const bool metal_rise = metal_detected && !prev_metal_for_edge;
  if (prox_rise || metal_rise) siren_hold_until_ms = now + SIREN_HOLD_MS;
  prev_proximity_for_edge = proximity_triggered;
  prev_metal_for_edge = metal_detected;

  const bool hold_active = static_cast<int32_t>(siren_hold_until_ms - now) > 0;
  siren_state = proximity_triggered || metal_detected || hold_active;
  const bool siren_level = SIREN_ACTIVE_HIGH ? siren_state : !siren_state;
  digitalWrite(SIREN_PIN, siren_level ? HIGH : LOW);
  digitalWrite(SIREN_PIN_SHIELD, siren_level ? HIGH : LOW);

  if (BUZZER_PIN < 0) return;  // No physical buzzer pin was specified in either source.
  if (!siren_state) {
    buzzer_output_on = false;
    digitalWrite(static_cast<uint8_t>(BUZZER_PIN), BUZZER_ACTIVE_HIGH ? LOW : HIGH);
    return;
  }
  if (now - last_buzzer_toggle_ms >= BUZZER_TOGGLE_MS) {
    last_buzzer_toggle_ms = now;
    buzzer_output_on = !buzzer_output_on;
    const bool level = BUZZER_ACTIVE_HIGH ? buzzer_output_on : !buzzer_output_on;
    digitalWrite(static_cast<uint8_t>(BUZZER_PIN), level ? HIGH : LOW);
  }
}

bool topLimitActive() {
  return digitalRead(LIMIT_SW_TOP) == LIMIT_SWITCH_ACTIVE_STATE;
}

bool bottomLimitActive() {
  return digitalRead(LIMIT_SW_BOTTOM) == LIMIT_SWITCH_ACTIVE_STATE;
}

char statusForTopLimit() {
  return TOP_LIMIT_MEANS_CLOSED ? 'c' : 'o';
}

char statusForBottomLimit() {
  return TOP_LIMIT_MEANS_CLOSED ? 'o' : 'c';
}

void stopGripperAt(char endpoint_status) {
  analogWrite(LIFT_PWM, 0);
  gripper_target_velocity = 0.0f;
  gripper_is_moving = false;
  gripper_status = endpoint_status;
}

void setMagnet(bool enabled) {
  magnet_state = enabled;
  digitalWrite(MAGNET_PIN, enabled ? HIGH : LOW);
}

void applyReadyMagnetPolicy() {
  const bool ready_policy = MAGNET_ALWAYS_ON_WHEN_DOWN || proximity_triggered || metal_detected;
  setMagnet(ready_policy);
}

const char *gripperStateName() {
  switch (grip_state) {
    case GRIP_BOOT: return "BOOT";
    case GRIP_DOWN_READY: return "DOWN_READY";
    case GRIP_MAGNET_SETTLE: return "MAGNET_SETTLE";
    case GRIP_MOVING_UP: return "MOVING_UP";
    case GRIP_TOP_RELEASE: return "TOP_RELEASE";
    case GRIP_MOVING_DOWN: return "MOVING_DOWN";
    case GRIP_RECOVERY_DOWN: return "RECOVERY_DOWN";
    case GRIP_FAULT: return "FAULT";
    default: return "UNKNOWN";
  }
}

void enterGripFault(const char *reason) {
  analogWrite(LIFT_PWM, 0);
  gripper_target_velocity = 0.0f;
  gripper_is_moving = false;
  grip_state = GRIP_FAULT;
  strncpy(grip_fault_reason, reason, sizeof(grip_fault_reason) - 1);
  grip_fault_reason[sizeof(grip_fault_reason) - 1] = '\0';
  strncpy(cycle_outcome, reason, sizeof(cycle_outcome) - 1);
  cycle_outcome[sizeof(cycle_outcome) - 1] = '\0';
  // Preserve the current magnet state on a travel fault: if still lifting a
  // mine, keep it attached. At release/top the state machine has already turned it off.
  gripper_status = 'm';
}

void initializeGripperState() {
  const bool top = topLimitActive();
  const bool bottom = bottomLimitActive();
  analogWrite(LIFT_PWM, 0);
  gripper_target_velocity = 0.0f;
  if (top && bottom) {
    setMagnet(false);
    enterGripFault("BOOT_BOTH_LIMITS");
  } else if (bottom && !top) {
    grip_state = GRIP_DOWN_READY;
    grip_state_started_ms = millis();
    gripper_status = statusForBottomLimit();
    applyReadyMagnetPolicy();
  } else {
    // Do not guess a homing direction at power-on. Move the mechanism safely
    // to the lower limit with power isolated, then reboot to re-arm the system.
    setMagnet(false);
    enterGripFault(top ? "BOOT_AT_TOP" : "BOOT_NOT_HOMED");
  }
}

bool requestGripCycle(const char *source) {
  if (grip_state != GRIP_DOWN_READY || !bottomLimitActive() || topLimitActive() ||
      fabs(gripper_target_velocity) >= 0.01f) {
    if (grip_state == GRIP_DOWN_READY) {
      strncpy(cycle_outcome, "REQUEST_REJECTED", sizeof(cycle_outcome) - 1);
      cycle_outcome[sizeof(cycle_outcome) - 1] = '\0';
    }
    return false;
  }
  (void)source;  // Source is intentionally not stored in the compact wire state.
  analogWrite(LIFT_PWM, 0);
  gripper_target_velocity = 0.0f;
  manual_lift_timing = false;
  setMagnet(true);
  grip_state = GRIP_MAGNET_SETTLE;
  grip_state_started_ms = millis();
  us_release_clear_count = 0;
  us_last_release_seq = us_sample_seq;
  cycle_release_verified = false;
  strncpy(cycle_outcome, "IN_PROGRESS", sizeof(cycle_outcome) - 1);
  cycle_outcome[sizeof(cycle_outcome) - 1] = '\0';
  auto_cycle_latched = true;
  return true;
}

bool requestRecoveryDown() {
  const bool top = topLimitActive();
  const bool bottom = bottomLimitActive();
  if (grip_state != GRIP_FAULT || bottom || (top && bottom)) return false;
  // Explicit recovery is operator-requested and only travels toward the lower
  // limit at the conservative cycle speed. The existing top/lower interlocks
  // and travel timeout remain active; normal gp commands cannot override it.
  analogWrite(LIFT_PWM, 0);
  gripper_target_velocity = 0.0f;
  manual_lift_timing = false;
  grip_state = GRIP_RECOVERY_DOWN;
  grip_state_started_ms = millis();
  strncpy(cycle_outcome, "RECOVERY_DOWN", sizeof(cycle_outcome) - 1);
  cycle_outcome[sizeof(cycle_outcome) - 1] = '\0';
  return true;
}

void beginCycleDown() {
  analogWrite(LIFT_PWM, 0);
  gripper_target_velocity = 0.0f;
  gripper_is_moving = false;
  grip_state = GRIP_DOWN_READY;
  grip_state_started_ms = millis();
  gripper_status = statusForBottomLimit();
  applyReadyMagnetPolicy();
  // Recalibrate the empty/down baseline after the cycle; don't infer that the
  // object is absent until new valid readings arrive.
  us_baseline_ready = false;
  us_baseline_accum = 0.0f;
  us_baseline_sample_count = 0;
  us_object_persist_count = 0;
  us_absent_persist_count = 0;
  us_object_present = false;
}

void recordUltrasonicSample(bool valid, float distance_cm) {
  ++us_sample_seq;
  us_last_reading_valid = valid;
  if (!valid) return;  // No echo is invalid, never evidence that a mine is present.

  us_last_valid_ms = millis();
  if (distance_cm > US_MAX_DISTANCE_CM) distance_cm = US_MAX_DISTANCE_CM;
  if (distance_cm < 0.0f) return;
  us_last_distance_cm = distance_cm;
  us_readings[us_reading_next] = distance_cm;
  us_reading_next = (us_reading_next + 1) % US_AVG_N;
  if (us_reading_count < US_AVG_N) ++us_reading_count;
  float sum = 0.0f;
  for (uint8_t i = 0; i < us_reading_count; ++i) sum += us_readings[i];
  us_filtered_distance_cm = sum / us_reading_count;
}

void updateUltrasonic() {
  const uint32_t now_us = micros();
  const uint32_t now_ms = millis();

  // All timing is bounded and non-blocking. Each state is revisited in loop().
  switch (us_state) {
    case US_IDLE:
      if (now_ms - last_us_sample_ms >= US_SAMPLE_INTERVAL_MS) {
        last_us_sample_ms = now_ms;
        digitalWrite(US_TRIG_PIN, HIGH);
        us_state_started_us = now_us;
        us_state = US_TRIGGER_HIGH;
      }
      break;
    case US_TRIGGER_HIGH:
      if (static_cast<uint32_t>(now_us - us_state_started_us) >= 10UL) {
        digitalWrite(US_TRIG_PIN, LOW);
        us_state_started_us = now_us;
        us_state = US_WAIT_RISE;
      }
      break;
    case US_WAIT_RISE:
      if (digitalRead(US_ECHO_PIN) == HIGH) {
        us_echo_rise_us = now_us;
        us_state = US_WAIT_FALL;
      } else if (static_cast<uint32_t>(now_us - us_state_started_us) >= US_ECHO_TIMEOUT_US) {
        recordUltrasonicSample(false, 0.0f);
        us_state = US_IDLE;
      }
      break;
    case US_WAIT_FALL:
      if (digitalRead(US_ECHO_PIN) == LOW) {
        const uint32_t duration_us = static_cast<uint32_t>(now_us - us_echo_rise_us);
        const float distance_cm = (static_cast<float>(duration_us) * 0.0343f) / 2.0f;
        if (duration_us == 0 || duration_us > US_ECHO_TIMEOUT_US || distance_cm < 2.0f) {
          recordUltrasonicSample(false, 0.0f);
        } else {
          recordUltrasonicSample(true, distance_cm);
        }
        us_state = US_IDLE;
      } else if (static_cast<uint32_t>(now_us - us_echo_rise_us) >= US_ECHO_TIMEOUT_US) {
        recordUltrasonicSample(false, 0.0f);
        us_state = US_IDLE;
      }
      break;
  }

  if (grip_state != GRIP_DOWN_READY || !bottomLimitActive() || topLimitActive()) return;
  if (us_sample_seq == us_last_baseline_seq) return;
  us_last_baseline_seq = us_sample_seq;
  if (!us_last_reading_valid || us_reading_count < US_AVG_N) return;

  if (!us_baseline_ready) {
    us_baseline_accum += us_filtered_distance_cm;
    if (++us_baseline_sample_count >= US_BASELINE_SAMPLES) {
      us_baseline_cm = us_baseline_accum / us_baseline_sample_count;
      us_baseline_ready = true;
    }
    return;
  }

  const float change_cm = fabs(us_filtered_distance_cm - us_baseline_cm);
  if (change_cm >= US_OBJECT_CHANGE_CM) {
    us_absent_persist_count = 0;
    if (us_object_persist_count < US_OBJECT_PERSISTENCE) ++us_object_persist_count;
    if (us_object_persist_count >= US_OBJECT_PERSISTENCE) us_object_present = true;
  } else {
    us_object_persist_count = 0;
    if (us_absent_persist_count < US_OBJECT_PERSISTENCE) ++us_absent_persist_count;
    if (us_absent_persist_count >= US_OBJECT_PERSISTENCE) {
      us_object_present = false;
      // Slowly track small empty-scene changes without following a large object step.
      us_baseline_cm = 0.98f * us_baseline_cm + 0.02f * us_filtered_distance_cm;
    }
  }

  const bool digital_gate_ok = !US_REQUIRE_DIGITAL_DETECTION_FOR_AUTO_CYCLE ||
                               proximity_triggered || metal_detected;
  if (ULTRASONIC_AUTO_CYCLE_ENABLED && us_object_present && digital_gate_ok && !auto_cycle_latched) {
    requestGripCycle("ultrasonic");
  }
  if (auto_cycle_latched && !us_object_present && !proximity_triggered && !metal_detected &&
      grip_state == GRIP_DOWN_READY) {
    auto_cycle_latched = false;
  }
}

void updateGripperMotor() {
  const uint32_t now = millis();
  const bool top = topLimitActive();
  const bool bottom = bottomLimitActive();

  // Contradictory endpoints always win over all requested motion.
  if (top && bottom) {
    analogWrite(LIFT_PWM, 0);
    gripper_target_velocity = 0.0f;
    gripper_is_moving = false;
    enterGripFault("BOTH_LIMITS_ACTIVE");
    return;
  }

  if (grip_state == GRIP_BOOT) initializeGripperState();
  if (grip_state == GRIP_FAULT) {
    analogWrite(LIFT_PWM, 0);
    gripper_target_velocity = 0.0f;
    gripper_is_moving = false;
    return;
  }

  if (grip_state == GRIP_RECOVERY_DOWN) {
    if (top && bottom) { enterGripFault("BOTH_LIMITS_ACTIVE"); return; }
    if (bottom) {
      analogWrite(LIFT_PWM, 0);
      gripper_target_velocity = 0.0f;
      gripper_is_moving = false;
      grip_state = GRIP_DOWN_READY;
      grip_state_started_ms = now;
      strncpy(grip_fault_reason, "NONE", sizeof(grip_fault_reason) - 1);
      grip_fault_reason[sizeof(grip_fault_reason) - 1] = '\0';
      strncpy(cycle_outcome, "RECOVERED_AT_BOTTOM", sizeof(cycle_outcome) - 1);
      cycle_outcome[sizeof(cycle_outcome) - 1] = '\0';
      gripper_status = statusForBottomLimit();
      applyReadyMagnetPolicy();
      return;
    }
    if (now - grip_state_started_ms >= MAX_LIFT_TRAVEL_MS) {
      enterGripFault("RECOVERY_DOWN_TIMEOUT");
      return;
    }
    digitalWrite(LIFT_DIR, LIFT_CLOSE_DIR_HIGH ? LOW : HIGH);
    analogWrite(LIFT_PWM, static_cast<uint8_t>((255UL * CYCLE_LIFT_PWM_PERCENT) / 100UL));
    gripper_is_moving = true;
    gripper_status = 'm';
    return;
  }

  // The one-cycle state machine exclusively owns the lift while active.
  if (grip_state == GRIP_MAGNET_SETTLE) {
    analogWrite(LIFT_PWM, 0);
    setMagnet(true);
    if (!bottom || top) { enterGripFault("BOTTOM_LOST_BEFORE_UP"); return; }
    if (now - grip_state_started_ms >= MAGNET_SETTLE_MS) {
      grip_state = GRIP_MOVING_UP;
      grip_state_started_ms = now;
    }
    return;
  }

  if (grip_state == GRIP_MOVING_UP) {
    if (top) {
      analogWrite(LIFT_PWM, 0);
      gripper_is_moving = false;
      gripper_status = statusForTopLimit();
      setMagnet(false);
      grip_state = GRIP_TOP_RELEASE;
      grip_state_started_ms = now;
      us_release_clear_count = 0;
      us_last_release_seq = us_sample_seq;
      return;
    }
    if (now - grip_state_started_ms >= MAX_LIFT_TRAVEL_MS) {
      enterGripFault("UP_TRAVEL_TIMEOUT");
      return;
    }
    digitalWrite(LIFT_DIR, LIFT_CLOSE_DIR_HIGH ? HIGH : LOW);
    analogWrite(LIFT_PWM, static_cast<uint8_t>((255UL * CYCLE_LIFT_PWM_PERCENT) / 100UL));
    gripper_is_moving = true;
    gripper_status = 'm';
    return;
  }

  if (grip_state == GRIP_TOP_RELEASE) {
    analogWrite(LIFT_PWM, 0);
    gripper_is_moving = false;
    setMagnet(false);
    if (ULTRASONIC_RELEASE_VERIFY_ENABLED) {
      if (us_sample_seq != us_last_release_seq) {
        us_last_release_seq = us_sample_seq;
        if (us_last_reading_valid && us_reading_count >= US_AVG_N &&
            us_filtered_distance_cm >= US_RELEASE_CLEAR_DISTANCE_CM) {
          if (us_release_clear_count < 3) ++us_release_clear_count;
        } else {
          us_release_clear_count = 0;
        }
      }
      if (us_release_clear_count >= 3) {
        cycle_release_verified = true;
        strncpy(cycle_outcome, "RELEASE_VERIFIED", sizeof(cycle_outcome) - 1);
        grip_state = GRIP_MOVING_DOWN;
        grip_state_started_ms = now;
      } else if (now - grip_state_started_ms >= RELEASE_VERIFY_TIMEOUT_MS) {
        enterGripFault("RELEASE_NOT_CONFIRMED");
      }
    } else if (now - grip_state_started_ms >= RELEASE_DWELL_MS) {
      cycle_release_verified = false;
      strncpy(cycle_outcome, "RELEASE_UNVERIFIED", sizeof(cycle_outcome) - 1);
      grip_state = GRIP_MOVING_DOWN;
      grip_state_started_ms = now;
    }
    return;
  }

  if (grip_state == GRIP_MOVING_DOWN) {
    if (bottom) {
      beginCycleDown();
      return;
    }
    if (now - grip_state_started_ms >= MAX_LIFT_TRAVEL_MS) {
      enterGripFault("DOWN_TRAVEL_TIMEOUT");
      return;
    }
    digitalWrite(LIFT_DIR, LIFT_CLOSE_DIR_HIGH ? LOW : HIGH);
    analogWrite(LIFT_PWM, static_cast<uint8_t>((255UL * CYCLE_LIFT_PWM_PERCENT) / 100UL));
    gripper_is_moving = true;
    gripper_status = 'm';
    return;
  }

  // Ready/manual mode: preserve existing gp/gn velocity control, with endpoints
  // and a maximum travel time still enforced. Never let manual commands override
  // the automatic cycle above.
  if (now - last_gripper_command_ms > GRIPPER_COMMAND_TIMEOUT_MS) {
    gripper_target_velocity = 0.0f;
  }
  if (fabs(gripper_target_velocity) < 0.01f) {
    analogWrite(LIFT_PWM, 0);
    gripper_is_moving = false;
    manual_lift_timing = false;
    if (bottom && !top) gripper_status = statusForBottomLimit();
    else if (top && !bottom) gripper_status = statusForTopLimit();
    else gripper_status = 'm';
    if (bottom && !top) {
      applyReadyMagnetPolicy();
    } else if (top && !bottom) {
      setMagnet(false);
    }
    return;
  }

  const bool closing = gripper_target_velocity < 0.0f;
  const bool close_limit = TOP_LIMIT_MEANS_CLOSED ? top : bottom;
  const bool open_limit = TOP_LIMIT_MEANS_CLOSED ? bottom : top;
  if ((closing && close_limit) || (!closing && open_limit)) {
    stopGripperAt(closing ? 'c' : 'o');
    manual_lift_timing = false;
    if (bottom && !top) applyReadyMagnetPolicy();
    else if (top && !bottom) setMagnet(false);
    return;
  }

  if (!manual_lift_timing || closing != manual_lift_last_closing) {
    manual_lift_timing = true;
    manual_lift_last_closing = closing;
    manual_lift_started_ms = now;
  }
  if (now - manual_lift_started_ms >= MAX_LIFT_TRAVEL_MS) {
    enterGripFault("MANUAL_TRAVEL_TIMEOUT");
    return;
  }

  const bool dir_high = closing ? LIFT_CLOSE_DIR_HIGH : !LIFT_CLOSE_DIR_HIGH;
  digitalWrite(LIFT_DIR, dir_high ? HIGH : LOW);
  analogWrite(LIFT_PWM, static_cast<uint8_t>((255UL * LIFT_PWM_PERCENT) / 100UL));
  gripper_is_moving = true;
  gripper_status = 'm';
}

int32_t getRightTotalPulses() {
  int32_t value;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { value = right_total_pulses; }
  return value;
}

int32_t getLeftTotalPulses() {
  int32_t value;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { value = left_total_pulses; }
  return value;
}

// -------------------- Camera servo, software PWM --------------------
uint32_t angleToPulse(int angle) {
  angle = clampInt(angle, CAMERA_MIN_ANGLE, CAMERA_MAX_ANGLE);
  return static_cast<uint32_t>(map(angle, 0, 180, SERVO_MIN_PULSE_US, SERVO_MAX_PULSE_US));
}

void setCameraAngle(int angle) {
  camera_angle = clampInt(angle, CAMERA_MIN_ANGLE, CAMERA_MAX_ANGLE);
  servo_pulse_width_us = angleToPulse(camera_angle);
}

void updateCameraServo() {
  uint32_t now_us = micros();

  if (!servo_pulse_high && static_cast<uint32_t>(now_us - servo_frame_start_us) >= SERVO_FRAME_US) {
    digitalWrite(CAMERA_SERVO_PIN, HIGH);
    servo_pulse_high = true;
    servo_frame_start_us = now_us;
  }

  if (servo_pulse_high && static_cast<uint32_t>(now_us - servo_frame_start_us) >= servo_pulse_width_us) {
    digitalWrite(CAMERA_SERVO_PIN, LOW);
    servo_pulse_high = false;
  }

  if (millis() - camera_last_command_ms > CAMERA_COMMAND_TIMEOUT_MS) camera_direction = 0;

  if (camera_direction != 0 && millis() - camera_last_command_ms <= CAMERA_COMMAND_TIMEOUT_MS) {
    if (millis() - last_camera_step_ms >= 30) {
      last_camera_step_ms = millis();
      setCameraAngle(camera_angle + camera_direction * CAMERA_STEP_DEG);
    }
  }
}

// -------------------- Serial --------------------
char token_buffer[64];
uint8_t token_index = 0;

void setWheelTarget(char wheel, char sign, float value) {
  if (sign == 'n') value = -value;
  value = clampFloat(value, -MAX_WHEEL_VELOCITY, MAX_WHEEL_VELOCITY);
  if (wheel == 'r') right_wheel.target_velocity = value;
  else if (wheel == 'l') left_wheel.target_velocity = value;
  last_command_ms = millis();
}

void processVelocityToken(const char *token) {
  const size_t len = strlen(token);
  if (len < 3) return;

  if ((token[0] == 'r' || token[0] == 'l') && (token[1] == 'p' || token[1] == 'n')) {
    setWheelTarget(token[0], token[1], atof(token + 2));
    return;
  }

  // Camera command from the current ROS-side interface.
  // sp1.00 = one direction, sn1.00 = opposite direction.
  if (token[0] == 's' && (token[1] == 'p' || token[1] == 'n')) {
    float v = atof(token + 2);
    if (v < 0.01f) {
      camera_direction = 0;
      return;
    }
    camera_direction = (token[1] == 'p') ? +1 : -1;
    camera_last_command_ms = millis();
    return;
  }

  // ROS2 roverInterface sends gp<value> / gn<value> continuously.
  // Negative is CLOSE and positive is OPEN to match the current teleop target log.
  if (token[0] == 'g' && (token[1] == 'p' || token[1] == 'n')) {
    // During a full cycle or a fault, ordinary ros2_control gp commands (including
    // continuous zeros) must not cancel/reverse the lift sequence.
    if (grip_state == GRIP_MAGNET_SETTLE || grip_state == GRIP_MOVING_UP ||
        grip_state == GRIP_TOP_RELEASE || grip_state == GRIP_MOVING_DOWN ||
        grip_state == GRIP_RECOVERY_DOWN || grip_state == GRIP_FAULT || grip_state == GRIP_BOOT) return;
    float value = atof(token + 2);
    if (token[1] == 'n') value = -value;
    gripper_target_velocity = clampFloat(value, -MAX_GRIPPER_COMMAND, MAX_GRIPPER_COMMAND);
    last_gripper_command_ms = millis();
    if (fabs(gripper_target_velocity) > 0.01f) gripper_status = 'm';
    return;
  }
}

void processExtendedToken(const char *token) {
  if (strncmp(token, "CSERVO:", 7) == 0) {
    setCameraAngle(atoi(token + 7));
    return;
  }

  if (strncmp(token, "SERVO:", 6) == 0) {
    setCameraAngle(atoi(token + 6));
    return;
  }

  // Direct serial test. Each command changes the target to a safe test velocity.
  if (strlen(token) == 1) {
    const float v = 2.0f;
    switch (token[0]) {
      case 'w': case 'W':
        right_wheel.target_velocity = +v;
        left_wheel.target_velocity = +v;
        last_command_ms = millis();
        break;
      case 's': case 'S':
        right_wheel.target_velocity = -v;
        left_wheel.target_velocity = -v;
        last_command_ms = millis();
        break;
      case 'a': case 'A':
        right_wheel.target_velocity = +v;
        left_wheel.target_velocity = -v;
        last_command_ms = millis();
        break;
      case 'd': case 'D':
        right_wheel.target_velocity = -v;
        left_wheel.target_velocity = +v;
        last_command_ms = millis();
        break;
      case 'x': case 'X':
        right_wheel.target_velocity = 0.0f;
        left_wheel.target_velocity = 0.0f;
        right_wheel.profiled_velocity = 0.0f;
        left_wheel.profiled_velocity = 0.0f;
        right_wheel.integral_term = 0.0f;
        left_wheel.integral_term = 0.0f;
        right_wheel.current_pwm = 0.0f;
        left_wheel.current_pwm = 0.0f;
        stopAllMotors();
        last_command_ms = millis();
        break;
      default:
        break;
    }
  }
}

void processToken(const char *token) {
  if (token[0] == '\0') return;

  // Additive, backward-compatible one-shot commands. These must be recognized
  // before routing the existing r/l/g/s velocity tokens.
  if (strcmp(token, "cycle:1") == 0) {
    requestGripCycle("serial");
    return;
  }
  if (strcmp(token, "grip:recover_down") == 0) {
    requestRecoveryDown();
    return;
  }
  if (strcmp(token, "grip:reset") == 0) {
    if (grip_state == GRIP_FAULT && !topLimitActive() && bottomLimitActive()) {
      grip_state = GRIP_DOWN_READY;
      strncpy(grip_fault_reason, "NONE", sizeof(grip_fault_reason) - 1);
      grip_fault_reason[sizeof(grip_fault_reason) - 1] = '\0';
      strncpy(cycle_outcome, "RESET_AT_BOTTOM", sizeof(cycle_outcome) - 1);
      cycle_outcome[sizeof(cycle_outcome) - 1] = '\0';
      gripper_target_velocity = 0.0f;
      manual_lift_timing = false;
      applyReadyMagnetPolicy();
    }
    return;
  }

  if (token[0] == 'C' || strncmp(token, "SERVO:", 6) == 0) {
    processExtendedToken(token);
    return;
  }

  if (token[0] == 'r' || token[0] == 'l' || token[0] == 'g' || token[0] == 's') {
    processVelocityToken(token);
    return;
  }

  processExtendedToken(token);
}

void readSerial() {
  while (Serial.available() > 0) {
    char c = static_cast<char>(Serial.read());

    if (c == ',' || c == '\n' || c == '\r') {
      if (token_index > 0) {
        token_buffer[token_index] = '\0';
        processToken(token_buffer);
        token_index = 0;
      }
      continue;
    }

    if (token_index < sizeof(token_buffer) - 1) token_buffer[token_index++] = c;
    else token_index = 0;
  }
}

// -------------------- Telemetry --------------------
void printSignedToken(char prefix, float value) {
  Serial.print(prefix);
  Serial.print(value >= 0.0f ? 'p' : 'n');
  Serial.print(fabs(value), 2);
}

void sendTelemetry() {
  // Keep the exact token names understood by roverInterface on EVERY frame.
  printSignedToken('r', right_wheel.measured_velocity);
  Serial.print(',');
  printSignedToken('l', left_wheel.measured_velocity);
  Serial.print(',');
  Serial.print('p');
  Serial.print(proximity_triggered ? '1' : '0');
  Serial.print(',');
  Serial.print('m');
  Serial.print(metal_detected ? '1' : '0');
  Serial.print(',');
  Serial.print('g');
  Serial.print(gripper_status);
  Serial.print(',');

  // Encoder totals are sent on every frame so roverInterface can update the
  // encoder joint positions and estimate their velocities at the 100 Hz loop.
  Serial.print("er:");
  Serial.print(getRightTotalPulses());
  Serial.print(",el:");
  Serial.print(getLeftTotalPulses());
  Serial.print(',');

  // Additive status fields. us is the five-sample filtered range (cm), uv marks
  // whether the latest echo was valid, cs is the FSM state, co is cycle outcome.
  Serial.print("us:");
  Serial.print(us_filtered_distance_cm, 1);
  Serial.print(",uv:");
  Serial.print((us_last_reading_valid && (millis() - us_last_valid_ms <= 250UL)) ? 1 : 0);
  Serial.print(",cs:");
  Serial.print(gripperStateName());
  Serial.print(",co:");
  Serial.print(cycle_outcome);
  Serial.print(",fr:");
  Serial.print(grip_fault_reason);
  Serial.print(",mg:");
  Serial.print(magnet_state ? 1 : 0);
  Serial.print(",si:");
  Serial.print(siren_state ? 1 : 0);
  Serial.print(',');

  // Raw digital proximity states are diagnostic only; the interface consumes p0/p1.
  static uint8_t diagnostic_divider = 0;
  diagnostic_divider = (diagnostic_divider + 1) % 5;
  if (diagnostic_divider == 0) {
    for (uint8_t i = 0; i < PROXIMITY_COUNT; ++i) {
      Serial.print('q');
      Serial.print(i + 1);
      Serial.print(':');
      Serial.print(proximity_values[i]);
      Serial.print(',');
    }
  }
  Serial.println();
}

// Optional raw debug line (not used by roverInterface).
void sendRawSensors() {
  Serial.print("P:");
  for (uint8_t i = 0; i < PROXIMITY_COUNT; ++i) {
    Serial.print(proximity_values[i]);
    if (i + 1 < PROXIMITY_COUNT) Serial.print(',');
  }
  Serial.print(" M:");
  Serial.println(metal_detected ? 1 : 0);
}

// -------------------- Setup / Loop --------------------
void setup() {
  Serial.begin(115200);

  pinMode(MOTOR_R_PWM, OUTPUT);   pinMode(MOTOR_R_DIR, OUTPUT);
  pinMode(MOTOR_L_PWM, OUTPUT);   pinMode(MOTOR_L_DIR, OUTPUT);
  pinMode(MOTOR_R_PWM_2, OUTPUT); pinMode(MOTOR_R_DIR_2, OUTPUT);
  pinMode(MOTOR_L_PWM_2, OUTPUT); pinMode(MOTOR_L_DIR_2, OUTPUT);
  stopAllMotors();

  pinMode(ENCODER_R_A, INPUT_PULLUP);
  pinMode(ENCODER_R_B, INPUT_PULLUP);
  pinMode(ENCODER_L_A, INPUT_PULLUP);
  pinMode(ENCODER_L_B, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(ENCODER_R_A), rightEncoderISR, RISING);
  attachInterrupt(digitalPinToInterrupt(ENCODER_L_A), leftEncoderISR, RISING);

  for (uint8_t i = 0; i < PROXIMITY_COUNT; ++i) pinMode(PROXIMITY_PINS[i], INPUT);
  pinMode(METAL_DETECTOR_PIN, INPUT);
  pinMode(METAL_DETECTOR_PIN_2, INPUT);

  pinMode(SIREN_PIN, OUTPUT);
  pinMode(SIREN_PIN_SHIELD, OUTPUT);
  digitalWrite(SIREN_PIN, SIREN_ACTIVE_HIGH ? LOW : HIGH);
  digitalWrite(SIREN_PIN_SHIELD, SIREN_ACTIVE_HIGH ? LOW : HIGH);
  pinMode(MAGNET_PIN, OUTPUT);
  digitalWrite(MAGNET_PIN, LOW);  // Keep electromagnet off at startup.

  pinMode(LIFT_PWM, OUTPUT);
  pinMode(LIFT_DIR, OUTPUT);
  analogWrite(LIFT_PWM, 0);
  pinMode(LIMIT_SW_TOP, INPUT);     // Requires external pull-downs as in reference firmware.
  pinMode(LIMIT_SW_BOTTOM, INPUT);

  pinMode(US_TRIG_PIN, OUTPUT);
  digitalWrite(US_TRIG_PIN, LOW);
  pinMode(US_ECHO_PIN, INPUT);

  if (BUZZER_PIN >= 0) {
    pinMode(static_cast<uint8_t>(BUZZER_PIN), OUTPUT);
    digitalWrite(static_cast<uint8_t>(BUZZER_PIN), BUZZER_ACTIVE_HIGH ? LOW : HIGH);
  }

  // Software servo output on D4.
  pinMode(CAMERA_SERVO_PIN, OUTPUT);
  digitalWrite(CAMERA_SERVO_PIN, LOW);
  setCameraAngle(CAMERA_CENTER_ANGLE);
  camera_last_command_ms = millis();
  servo_frame_start_us = micros();
  last_camera_step_ms = millis();

  last_control_ms = millis();
  last_telemetry_ms = millis();
  last_sensor_ms = millis();
  last_command_ms = millis();
  last_gripper_command_ms = millis();
  metal_last_raw = (digitalRead(METAL_DETECTOR_PIN) == METAL_ACTIVE_STATE);
  metal_detected = metal_last_raw;
  metal_last_change_ms = millis();
  updateSensors();
  updateAlarmOutputs();
  last_us_sample_ms = millis();
  initializeGripperState();  // No automatic homing movement on startup.

  // This one startup line is harmless: roverInterface ignores unknown tokens.
  Serial.println("ROVER_ARDUINO_READY");
}

void loop() {
  readSerial();
  updateCameraServo();
  updateUltrasonic();
  updateGripperMotor();

  uint32_t now = millis();

  if (now - last_control_ms >= CONTROL_PERIOD_MS) {
    last_control_ms += CONTROL_PERIOD_MS;
    updateControl();
  }

  if (now - last_sensor_ms >= SENSOR_PERIOD_MS) {
    last_sensor_ms = now;
    updateSensors();
    updateAlarmOutputs();
  }

  if (now - last_telemetry_ms >= TELEMETRY_PERIOD_MS) {
    last_telemetry_ms += TELEMETRY_PERIOD_MS;
    sendTelemetry();
  }
}