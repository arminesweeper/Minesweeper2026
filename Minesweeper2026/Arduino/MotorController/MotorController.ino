/**
 * ============================================================================
 * MOTORCONTROLLER.INO - Main Application Entry Point
 * ============================================================================
 * Minesweeper Robot Motor Controller
 *
 * Hardware:
 *   - Arduino Mega 2560 (ATmega2560-16U2)
 *   - Cytron MDD10A Rev 2.0 Motor Driver
 *   - Dual Quadrature Encoders
 *
 * Architecture:
 *   - Modular multi-file design
 *   - Non-blocking control loop
 *   - Hardware abstraction layers
 *   - Safety monitoring with watchdog
 * ============================================================================
 */

#include "Config.h"
#include "Diagnostics.h"
#include "Encoder.h"
#include "MotionController.h"
#include "MotorDriver.h"
#include "Odometry.h"
#include "PIDController.h"
#include "Safety.h"
#include "SerialProtocol.h"


// ============================================================================
// GLOBAL OBJECT INSTANTIATION
// ============================================================================

// Motor Driver Configuration
const MotorPinConfig right_motor_config = {Pins::MOTOR_R_PWM,
                                           Pins::MOTOR_R_DIR};

const MotorPinConfig left_motor_config = {Pins::MOTOR_L_PWM, Pins::MOTOR_L_DIR};

// Motor Drivers (left motor typically inverted due to mounting)
MotorDriver motorRight(right_motor_config, false);
MotorDriver motorLeft(left_motor_config, true); // Inverted

// Encoder Configuration
const EncoderPinConfig right_encoder_config = {
    Pins::ENCODER_R_A, Pins::ENCODER_R_B,
    5 // INT5
};

const EncoderPinConfig left_encoder_config = {
    Pins::ENCODER_L_A, Pins::ENCODER_L_B,
    4 // INT4
};

// Encoders (left encoder inverted to match motor inversion)
Encoder encoderRight(right_encoder_config, false);
Encoder encoderLeft(left_encoder_config, true); // Inverted

// Set static instance pointers for ISR access
Encoder *Encoder::instance_right_ = &encoderRight;
Encoder *Encoder::instance_left_ = &encoderLeft;

// PID Controllers
PIDController pidRight(PIDTuning::KP_RIGHT, PIDTuning::KI_RIGHT,
                       PIDTuning::KD_RIGHT, PIDTuning::OUTPUT_MIN,
                       PIDTuning::OUTPUT_MAX,
                       static_cast<int>(Timing::CONTROL_INTERVAL_MS));

PIDController pidLeft(PIDTuning::KP_LEFT, PIDTuning::KI_LEFT,
                      PIDTuning::KD_LEFT, PIDTuning::OUTPUT_MIN,
                      PIDTuning::OUTPUT_MAX,
                      static_cast<int>(Timing::CONTROL_INTERVAL_MS));

// Serial Protocol Handler
SerialProtocol serialProtocol;

// Motion Controller
MotionController motionController;

// Safety Monitor
SafetyMonitor safetyMonitor;

// Diagnostics
Diagnostics diagnostics;

// Odometry
Odometry odometry;

// Timing
unsigned long last_control_time_ms = 0;
unsigned long last_telemetry_time_ms = 0;

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

void initializeHardware();
void processControlLoop();
void processTelemetry();
void handleSafetyState(SystemState state);

// ============================================================================
// SETUP - System Initialization
// ============================================================================

void setup() {
  // Initialize all subsystems
  initializeHardware();

  // Send startup message
  serialProtocol.sendStatus("Minesweeper Motor Controller v2.0");
  serialProtocol.sendStatus("READY");

  // Initialize timing
  last_control_time_ms = millis();
  last_telemetry_time_ms = millis();
}

// ============================================================================
// LOOP - Main Execution
// ============================================================================

void loop() {
  // Start diagnostics timing
  diagnostics.loopStart();

  // Reset watchdog (must be called frequently)
  safetyMonitor.resetWatchdog();

  // Process incoming serial commands
  double cmd_right_vel = 0.0;
  double cmd_left_vel = 0.0;

  if (serialProtocol.processInput(cmd_right_vel, cmd_left_vel)) {
    // Valid command received
    motionController.setRightTarget(cmd_right_vel);
    motionController.setLeftTarget(cmd_left_vel);
    diagnostics.incrementCommandCount();

    // Clear timeout if in timeout state
    if (safetyMonitor.getState() == SystemState::ESTOP_TIMEOUT) {
      safetyMonitor.clearEStop();
    }
  }

  // Run control loop at fixed interval
  unsigned long current_time = millis();
  if (current_time - last_control_time_ms >= Timing::CONTROL_INTERVAL_MS) {
    double dt = (current_time - last_control_time_ms) / 1000.0;
    last_control_time_ms = current_time;

    processControlLoop();
    diagnostics.incrementControlCycleCount();
  }

  // Send telemetry at fixed interval
  if (current_time - last_telemetry_time_ms >= Timing::TELEMETRY_INTERVAL_MS) {
    last_telemetry_time_ms = current_time;
    processTelemetry();
  }

  // Update safety monitor
  SystemState current_state =
      safetyMonitor.update(serialProtocol.getLastCommandTime(),
                           motionController.getRightState().measured_velocity,
                           motionController.getLeftState().measured_velocity);

  // Handle safety state transitions
  handleSafetyState(current_state);

  // Send periodic diagnostics
  diagnostics.sendStatusReport(safetyMonitor.getState(),
                               safetyMonitor.getFaults());

  // End diagnostics timing
  diagnostics.loopEnd();
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void initializeHardware() {
  // Initialize motor drivers
  motorRight.begin();
  motorLeft.begin();

  // Initialize encoders with ISR functions
  encoderRight.begin(rightEncoderISR);
  encoderLeft.begin(leftEncoderISR);

  // Initialize PID controllers
  pidRight.begin();
  pidLeft.begin();

  // Initialize serial protocol
  serialProtocol.begin();

  // Initialize motion controller
  motionController.begin();

  // Initialize safety monitor (enables watchdog)
  safetyMonitor.begin();

  // Initialize diagnostics
  diagnostics.begin();

  // Initialize odometry
  odometry.begin();

  // Ensure motors are stopped
  motorRight.stop();
  motorLeft.stop();
}

// ============================================================================
// CONTROL LOOP
// ============================================================================

void processControlLoop() {
  // Get elapsed time
  double dt = Timing::CONTROL_INTERVAL_S;

  // Read encoder pulses (atomic operation)
  int32_t right_pulses = encoderRight.getPulsesAndReset();
  int32_t left_pulses = encoderLeft.getPulsesAndReset();

  // Calculate measured velocities
  double right_meas_vel = encoderRight.calculateVelocity(right_pulses, dt);
  double left_meas_vel = encoderLeft.calculateVelocity(left_pulses, dt);

  // Update motion state with measured velocities
  motionController.getRightStateMutable().measured_velocity = right_meas_vel;
  motionController.getLeftStateMutable().measured_velocity = left_meas_vel;

  // Update velocity profiles (ramping)
  motionController.updateProfiles(dt);

  // Compute PID outputs
  double right_pid_out =
      pidRight.compute(motionController.getRightState().profiled_velocity,
                       motionController.getRightState().measured_velocity);

  double left_pid_out =
      pidLeft.compute(motionController.getLeftState().profiled_velocity,
                      motionController.getLeftState().measured_velocity);

  // Update motion state with PID outputs
  motionController.getRightStateMutable().pid_output = right_pid_out;
  motionController.getLeftStateMutable().pid_output = left_pid_out;

  // Apply deadband compensation
  motionController.applyDeadband();

  // Apply outputs to motors (only if active)
  if (safetyMonitor.isActive()) {
    motorRight.setOutput(motionController.getRightState().pid_output);
    motorLeft.setOutput(motionController.getLeftState().pid_output);
  }

  // Update odometry
  odometry.update(right_meas_vel, left_meas_vel, dt);
}

// ============================================================================
// TELEMETRY
// ============================================================================

void processTelemetry() {
  // Send velocity telemetry
  serialProtocol.sendTelemetry(
      motionController.getRightState().measured_velocity,
      motionController.getLeftState().measured_velocity);

  // Send odometry (if enabled)
#if ENABLE_ODOMETRY
  serialProtocol.sendOdometry(odometry.getX(), odometry.getY(),
                              odometry.getTheta());
#endif
}

// ============================================================================
// SAFETY STATE HANDLING
// ============================================================================

void handleSafetyState(SystemState state) {
  switch (state) {
  case SystemState::ACTIVE:
    // Normal operation - do nothing special
    break;

  case SystemState::ESTOP_TIMEOUT:
  case SystemState::ESTOP_MANUAL:
  case SystemState::FAULT_ENCODER:
  case SystemState::FAULT_BATTERY:
  case SystemState::FAULT_UNKNOWN:
    // Any non-active state - ensure motors are stopped
    motorRight.emergencyStop();
    motorLeft.emergencyStop();
    motionController.emergencyStop();

    // Increment timeout counter if this is a timeout
    if (state == SystemState::ESTOP_TIMEOUT) {
      static bool timeout_counted = false;
      if (!timeout_counted) {
        diagnostics.incrementTimeoutCount();
        timeout_counted = true;
      }
    } else {
      // Reset timeout counted flag when not in timeout state
      static_cast<void>(false); // No-op, placeholder for clarity
    }
    break;

  default:
    motorRight.emergencyStop();
    motorLeft.emergencyStop();
    break;
  }
}