/**
 * ============================================================================
 * CONFIG.H - System Configuration & Hardware Definitions
 * ============================================================================
 * Central configuration file for the Minesweeper Robot Motor Controller.
 * All tunable parameters and hardware pin assignments are defined here.
 * ============================================================================
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ============================================================================
// COMPILE-TIME FEATURE FLAGS
// ============================================================================
#define ENABLE_WATCHDOG 1       // Enable hardware watchdog timer
#define ENABLE_ODOMETRY 1       // Enable position tracking
#define ENABLE_DIAGNOSTICS 1    // Enable detailed debug output
#define ENABLE_SAFETY_MONITOR 1 // Enable timeout and fault detection

// ============================================================================
// HARDWARE PIN DEFINITIONS - Cytron MDD10A Rev 2.0
// ============================================================================
namespace Pins {
// Motor Driver (Sign-Magnitude Mode)
constexpr uint8_t MOTOR_R_PWM = 9;  // Timer 2 (8-bit PWM)
constexpr uint8_t MOTOR_R_DIR = 12; // Direction control
constexpr uint8_t MOTOR_L_PWM = 11; // Timer 1 (16-bit PWM)
constexpr uint8_t MOTOR_L_DIR = 7;  // Direction control

// Right Wheel Encoder
constexpr uint8_t ENCODER_R_A = 3; // INT5 (External Interrupt)
constexpr uint8_t ENCODER_R_B = 5; // General Digital I/O

// Left Wheel Encoder
constexpr uint8_t ENCODER_L_A = 2; // INT4 (External Interrupt)
constexpr uint8_t ENCODER_L_B = 4; // General Digital I/O

// Optional: Battery Voltage Monitoring
constexpr uint8_t BATTERY_SENSE = A0; // Analog input for voltage divider
} // namespace Pins

// ============================================================================
// ENCODER SPECIFICATIONS
// ============================================================================
namespace EncoderSpec {
constexpr double PPR = 385.0; // Pulses Per Revolution
constexpr double RADIANS_PER_PULSE = (2.0 * 3.14159265359) / PPR;
constexpr double RPM_TO_RADS = 0.104719755; // (2 * PI) / 60
} // namespace EncoderSpec

// ============================================================================
// ROBOT PHYSICAL PARAMETERS (For Odometry)
// ============================================================================
namespace RobotParams {
constexpr double WHEEL_DIAMETER_MM = 65.0; // Wheel diameter in mm
constexpr double WHEEL_RADIUS_M = (WHEEL_DIAMETER_MM / 1000.0) / 2.0;
constexpr double WHEEL_BASE_MM = 150.0; // Distance between wheels
constexpr double WHEEL_BASE_M = WHEEL_BASE_MM / 1000.0;
constexpr double TICKS_PER_METER =
    EncoderSpec::PPR / (3.14159265359 * WHEEL_DIAMETER_MM / 1000.0);
} // namespace RobotParams

// ============================================================================
// CONTROL LOOP TIMING
// ============================================================================
namespace Timing {
constexpr unsigned long CONTROL_INTERVAL_MS = 100; // Main loop period (ms)
constexpr double CONTROL_INTERVAL_S = CONTROL_INTERVAL_MS / 1000.0;
constexpr unsigned long TELEMETRY_INTERVAL_MS = 100; // Telemetry output rate
} // namespace Timing

// ============================================================================
// PID TUNING PARAMETERS
// ============================================================================
namespace PIDTuning {
// Right Motor PID Gains
constexpr double KP_RIGHT = 11.5;
constexpr double KI_RIGHT = 7.5;
constexpr double KD_RIGHT = 0.1;

// Left Motor PID Gains
constexpr double KP_LEFT = 12.8;
constexpr double KI_LEFT = 8.3;
constexpr double KD_LEFT = 0.1;

// Output Limits
constexpr double OUTPUT_MIN = -255.0;
constexpr double OUTPUT_MAX = 255.0;

// Anti-Windup
constexpr double INTEGRAL_WINDUP_LIMIT = 200.0;
} // namespace PIDTuning

// ============================================================================
// MOTION CONSTRAINTS
// ============================================================================
namespace MotionLimits {
constexpr double MAX_ACCELERATION = 15.0;   // Max accel (rad/s²)
constexpr double MAX_DECELERATION = 20.0;   // Max decel (rad/s²)
constexpr double MIN_OUTPUT_DEADBAND = 8.0; // PWM to overcome stiction
constexpr double VELOCITY_TOLERANCE = 0.01; // Near-zero threshold (rad/s)
} // namespace MotionLimits

// ============================================================================
// SAFETY PARAMETERS
// ============================================================================
namespace Safety {
constexpr unsigned long COMMAND_TIMEOUT_MS = 1500;  // Serial timeout (ms)
constexpr unsigned long WATCHDOG_TIMEOUT_MS = 2000; // WDT reset (ms)
constexpr double MAX_VELOCITY_RAD_S = 10.0;         // Velocity sanity limit
constexpr uint16_t BATTERY_VOLTAGE_LOW = 10800;     // 10.8V in millivolts
constexpr uint16_t BATTERY_VOLTAGE_HIGH = 16800;    // 16.8V in millivolts
constexpr double VOLTAGE_DIVIDER_RATIO = 3.3;       // R1+R2/R2
} // namespace Safety

// ============================================================================
// SERIAL COMMUNICATION
// ============================================================================
namespace SerialConfig {
constexpr unsigned long BAUD_RATE = 115200;
constexpr size_t RX_BUFFER_SIZE = 32;
constexpr size_t TX_BUFFER_SIZE = 64;
constexpr char PACKET_DELIMITER = '\n';
} // namespace SerialConfig

// ============================================================================
// DIAGNOSTICS SETTINGS
// ============================================================================
namespace DiagConfig {
constexpr unsigned long STATUS_INTERVAL_MS = 5000; // Periodic status output
constexpr bool VERBOSE_ENCODER = false;
constexpr bool VERBOSE_PID = false;
constexpr bool VERBOSE_SAFETY = true;
} // namespace DiagConfig

#endif // CONFIG_H