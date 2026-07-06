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
 *   - MPU6050 IMU
 *   - Lift Mechanism with 5 Electromagnets
 *   - 5 Analog Proximity Sensors, Digital Metal Detector
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
#include "IMU.h"
#include "LiftController.h"
#include "MotionController.h"
#include "MotorDriver.h"
#include "Odometry.h"
#include "PIDController.h"
#include "Safety.h"
#include "Sensors.h"
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

// ISR forward declarations (defined in Encoder.cpp)
extern void rightEncoderISR();
extern void leftEncoderISR();

// PID Controllers
PIDController pidRight(PIDTuning::KP_RIGHT, PIDTuning::KI_RIGHT,
                       PIDTuning::KD_RIGHT, PIDTuning::OUTPUT_MIN,
                       PIDTuning::OUTPUT_MAX,
                       static_cast<int>(Timing::CONTROL_INTERVAL_MS));

PIDController pidLeft(PIDTuning::KP_LEFT, PIDTuning::KI_LEFT,
                      PIDTuning::KD_LEFT, PIDTuning::OUTPUT_MIN,
                      PIDTuning::OUTPUT_MAX,
                      static_cast<int>(Timing::CONTROL_INTERVAL_MS));

// Serial Protocol Handler (Core Subsystems)
SerialProtocol serialProtocol;

// Motion Controller
MotionController motionController;

// Safety Monitor
SafetyMonitor safetyMonitor;

// Diagnostics
Diagnostics diagnostics;

// Odometry
Odometry odometry;

// Peripheral Subsystems
IMU imu;
LiftController liftController;
Sensors sensors;

// Timing Variables
unsigned long last_control_time_ms = 0;
unsigned long last_telemetry_time_ms = 0;
unsigned long last_imu_time_ms = 0;
unsigned long last_sensor_time_ms = 0;
unsigned long last_lift_time_ms = 0;

// Metal detector latch (to ensure we send the telemetry message exactly once per trigger)
bool last_metal_detect_state = false;

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

void initializeHardware();
void processControlLoop();
void processTelemetry();
void handleSafetyState(SystemState state);
void processExtendedCommand();

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
  unsigned long now = millis();
  last_control_time_ms = now;
  last_telemetry_time_ms = now;
  last_imu_time_ms = now;
  last_sensor_time_ms = now;
  last_lift_time_ms = now;
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
    // Valid velocity command received
    motionController.setRightTarget(cmd_right_vel);
    motionController.setLeftTarget(cmd_left_vel);
    diagnostics.incrementCommandCount();

    // Clear timeout if in timeout state
    if (safetyMonitor.getState() == SystemState::ESTOP_TIMEOUT) {
      safetyMonitor.clearEStop();
    }
    
    // Acknowledge command with a beep
    sensors.beep();
  }

  // Process extended commands
  processExtendedCommand();

  unsigned long current_time = millis();

  // 1. Run control loop (10 Hz)
  if (current_time - last_control_time_ms >= Timing::CONTROL_INTERVAL_MS) {
    last_control_time_ms = current_time;
    processControlLoop();
    diagnostics.incrementControlCycleCount();
  }

  // 2. Read IMU (50 Hz)
#if ENABLE_IMU
  if (current_time - last_imu_time_ms >= Timing::IMU_INTERVAL_MS) {
    double dt = (current_time - last_imu_time_ms) / 1000.0;
    last_imu_time_ms = current_time;
    imu.update(dt);
    safetyMonitor.setIMUFault(imu.hasError());
  }
#endif

  // 3. Read Sensors (20 Hz)
#if ENABLE_SENSORS
  if (current_time - last_sensor_time_ms >= Timing::SENSOR_INTERVAL_MS) {
    last_sensor_time_ms = current_time;
    sensors.update();

    // Send metal detect message on rising edge, and trigger buzzer
    bool current_metal = sensors.isMetalDetected();
    if (current_metal && !last_metal_detect_state) {
        serialProtocol.sendMetalDetect(true);
        sensors.setBuzzerPattern(BuzzerPattern::MINE_DETECT);
    } else if (!current_metal && last_metal_detect_state) {
        serialProtocol.sendMetalDetect(false);
        sensors.setBuzzerPattern(BuzzerPattern::SILENT);
    }
    last_metal_detect_state = current_metal;
  }
#endif

  // 4. Update Lift Controller (20 Hz)
#if ENABLE_LIFT
  if (current_time - last_lift_time_ms >= Timing::LIFT_INTERVAL_MS) {
    last_lift_time_ms = current_time;
    LiftState prev_state = liftController.getState();
    liftController.update();
    
    // Check for lift faults
    safetyMonitor.setLiftFault(liftController.hasFault());

    // Send telemetry on state change
    if (liftController.getState() != prev_state) {
        serialProtocol.sendLiftState(liftController.getStateString(), liftController.getMagnetState());
    }
  }
#endif

  // 5. Send telemetry (10 Hz)
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
  // Set ISR instance pointers
  Encoder::instance_right_ = &encoderRight;
  Encoder::instance_left_ = &encoderLeft;

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

  // Initialize diagnostics
  diagnostics.begin();

  // Initialize odometry
  odometry.begin();

#if ENABLE_IMU
  if (!imu.begin()) {
    serialProtocol.sendError(3, "IMU init failed");
  }
#endif

#if ENABLE_SENSORS
  sensors.begin();
#endif

#if ENABLE_LIFT
  liftController.begin();
#endif

  // Initialize safety monitor last (enables watchdog)
  safetyMonitor.begin();

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
// EXTENDED COMMAND PROCESSING
// ============================================================================

void processExtendedCommand() {
  char cmd_buf[SerialConfig::CMD_BUFFER_SIZE];
  if (serialProtocol.getExtendedCommand(cmd_buf, sizeof(cmd_buf))) {
    
    if (strncmp(cmd_buf, "CLIFT:", 6) == 0) {
      if (strcmp(cmd_buf, "CLIFT:UP") == 0) liftController.raise();
      else if (strcmp(cmd_buf, "CLIFT:DN") == 0) liftController.lower();
      else if (strcmp(cmd_buf, "CLIFT:STOP") == 0) liftController.stop();
    } 
    else if (strncmp(cmd_buf, "CMAG:", 5) == 0) {
      if (strcmp(cmd_buf, "CMAG:ALL:ON") == 0) liftController.setAllMagnets(true);
      else if (strcmp(cmd_buf, "CMAG:ALL:OFF") == 0) liftController.setAllMagnets(false);
      else {
        // Parse single magnet command e.g. "CMAG:1:ON"
        int mag_num = cmd_buf[5] - '1'; // 0-based index
        if (mag_num >= 0 && mag_num < 5) {
            bool on = (strcmp(&cmd_buf[7], "ON") == 0);
            liftController.setMagnet(mag_num, on);
        }
      }
    }
    else if (strncmp(cmd_buf, "CBUZZ:", 6) == 0) {
      if (strcmp(cmd_buf, "CBUZZ:SILENT") == 0) sensors.setBuzzerPattern(BuzzerPattern::SILENT);
      else if (strcmp(cmd_buf, "CBUZZ:BEEP") == 0) sensors.beep();
      else if (strcmp(cmd_buf, "CBUZZ:ALERT") == 0) sensors.setBuzzerPattern(BuzzerPattern::ALERT);
      else if (strcmp(cmd_buf, "CBUZZ:ALARM") == 0) sensors.setBuzzerPattern(BuzzerPattern::ALARM);
    }
    else if (strcmp(cmd_buf, "CRESET") == 0) {
      odometry.reset();
      imu.resetYaw();
      diagnostics.resetStats();
      serialProtocol.sendStatus("Reset complete");
    }
    else if (strcmp(cmd_buf, "CDIAG") == 0) {
      diagnostics.sendStatusReport(safetyMonitor.getState(), safetyMonitor.getFaults());
    }
    else if (strcmp(cmd_buf, "CESTOP") == 0) {
      safetyMonitor.triggerEStop();
    }
    else if (strcmp(cmd_buf, "CCLEAR") == 0) {
      safetyMonitor.clearEStop();
      liftController.clearFault();
    }
    else {
      serialProtocol.sendError(1, "Unknown extended command");
    }
  }
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

#if ENABLE_IMU
  serialProtocol.sendIMU(imu.getYaw(), imu.getPitch(), imu.getRoll());
#endif

#if ENABLE_SENSORS
  uint16_t prox[5];
  sensors.getAllProximity(prox);
  serialProtocol.sendProximity(prox, 5);
#endif
}

// ============================================================================
// SAFETY STATE HANDLING
// ============================================================================

void handleSafetyState(SystemState state) {
  switch (state) {
  case SystemState::ACTIVE:
    // Normal operation
    sensors.setLEDPattern(LEDPattern::OFF);
    break;

  case SystemState::ESTOP_TIMEOUT:
  case SystemState::ESTOP_MANUAL:
    motorRight.emergencyStop();
    motorLeft.emergencyStop();
    motionController.emergencyStop();
    sensors.setLEDPattern(LEDPattern::SLOW_BLINK);
    
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

  case SystemState::FAULT_LIFT:
    liftController.stop();
    // Fall-through intended to stop drive motors as well
  case SystemState::FAULT_ENCODER:
  case SystemState::FAULT_BATTERY:
  case SystemState::FAULT_UNKNOWN:
  case SystemState::FAULT_IMU:
    motorRight.emergencyStop();
    motorLeft.emergencyStop();
    motionController.emergencyStop();
    sensors.setLEDPattern(LEDPattern::FAST_BLINK);
    break;

  default:
    motorRight.emergencyStop();
    motorLeft.emergencyStop();
    break;
  }
}