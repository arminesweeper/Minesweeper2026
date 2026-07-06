/**
 * ============================================================================
 * CONFIG.H - System Configuration & Hardware Definitions
 * ============================================================================
 * Central configuration file for the Minesweeper Robot Motor Controller.
 * All tunable parameters, hardware pin assignments, and feature flags are
 * defined here.  No other file should contain magic numbers or pin literals.
 *
 * @file   Config.h
 * @author Assiut Robotics Team
 * @date   2026
 * ============================================================================
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

/* ============================================================================
 * COMPILE-TIME FEATURE FLAGS
 * ============================================================================ */

/** @brief Enable ATmega2560 hardware watchdog timer (2-second timeout). */
#define ENABLE_WATCHDOG         1

/** @brief Enable differential-drive odometry (x, y, theta tracking). */
#define ENABLE_ODOMETRY         1

/** @brief Enable periodic diagnostics output via serial. */
#define ENABLE_DIAGNOSTICS      1

/** @brief Enable safety monitoring (command timeout, fault detection). */
#define ENABLE_SAFETY_MONITOR   1

/** @brief Enable MPU6050 IMU integration (yaw, pitch, roll). */
#define ENABLE_IMU              1

/** @brief Enable lift mechanism controller (DC motor + limit switches). */
#define ENABLE_LIFT             1

/** @brief Enable sensor subsystem (proximity, metal detector, buzzer, LED). */
#define ENABLE_SENSORS          1

/* ============================================================================
 * HARDWARE PIN DEFINITIONS
 * ============================================================================ */

/**
 * @brief Pin assignments for all hardware connected to the Arduino Mega 2560.
 *
 * Grouped by subsystem.  Timer conflicts and interrupt assignments are
 * documented inline.
 */
namespace Pins {

/* --- Motor Driver: Cytron MDD10A Rev 2.0 (Sign-Magnitude Mode) ----------- */

/** @brief Right motor PWM output (Timer 2, OC2B, 8-bit, ~490 Hz). */
constexpr uint8_t MOTOR_R_PWM = 9;

/** @brief Right motor direction pin (HIGH = forward, LOW = reverse). */
constexpr uint8_t MOTOR_R_DIR = 12;

/** @brief Left motor PWM output (Timer 1, OC1A, 16-bit, ~490 Hz). */
constexpr uint8_t MOTOR_L_PWM = 11;

/** @brief Left motor direction pin (HIGH = forward, LOW = reverse). */
constexpr uint8_t MOTOR_L_DIR = 7;

/* --- Wheel Encoders ------------------------------------------------------- */

/** @brief Right encoder Phase A (INT5, external interrupt, RISING). */
constexpr uint8_t ENCODER_R_A = 3;

/** @brief Right encoder Phase B (general digital input with pull-up). */
constexpr uint8_t ENCODER_R_B = 5;

/** @brief Left encoder Phase A (INT4, external interrupt, RISING). */
constexpr uint8_t ENCODER_L_A = 2;

/** @brief Left encoder Phase B (general digital input with pull-up). */
constexpr uint8_t ENCODER_L_B = 4;

/* --- IMU: MPU6050 via I2C ------------------------------------------------- */

/** @brief I2C data line (Wire library default on Mega). */
constexpr uint8_t IMU_SDA = 20;

/** @brief I2C clock line (Wire library default on Mega). */
constexpr uint8_t IMU_SCL = 21;

/* --- Lift Mechanism ------------------------------------------------------- */

/** @brief Lift motor PWM output (Timer 2, OC2A, ~490 Hz). */
constexpr uint8_t LIFT_PWM = 10;

/** @brief Lift motor direction pin (HIGH = up, LOW = down). */
constexpr uint8_t LIFT_DIR = 8;

/** @brief Top limit switch input (active LOW, internal pull-up enabled). */
constexpr uint8_t LIMIT_SW_TOP = 28;

/** @brief Bottom limit switch input (active LOW, internal pull-up enabled). */
constexpr uint8_t LIMIT_SW_BOTTOM = 29;

/* --- Electromagnets (5 channels, via MOSFET/relay drivers) ---------------- */

constexpr uint8_t MAGNET_1 = 22;  ///< Electromagnet channel 1
constexpr uint8_t MAGNET_2 = 23;  ///< Electromagnet channel 2
constexpr uint8_t MAGNET_3 = 24;  ///< Electromagnet channel 3
constexpr uint8_t MAGNET_4 = 25;  ///< Electromagnet channel 4
constexpr uint8_t MAGNET_5 = 26;  ///< Electromagnet channel 5

/** @brief Array of all magnet pins for indexed access. */
constexpr uint8_t MAGNET_PINS[] = {22, 23, 24, 25, 26};

/* --- Sensors -------------------------------------------------------------- */

/** @brief Metal detector digital input (active LOW, internal pull-up). */
constexpr uint8_t METAL_DETECTOR = 27;

/** @brief Proximity sensor analog inputs (ADC, 0-1023). */
constexpr uint8_t PROXIMITY_1 = A1;
constexpr uint8_t PROXIMITY_2 = A2;
constexpr uint8_t PROXIMITY_3 = A3;
constexpr uint8_t PROXIMITY_4 = A4;
constexpr uint8_t PROXIMITY_5 = A5;

/** @brief Array of all proximity sensor pins for indexed access. */
constexpr uint8_t PROXIMITY_PINS[] = {A1, A2, A3, A4, A5};

/* --- Indicators ----------------------------------------------------------- */

/** @brief Buzzer output (Timer 4, OC4A, PWM capable for tone()). */
constexpr uint8_t BUZZER = 6;

/** @brief Warning LED output (also Arduino built-in LED). */
constexpr uint8_t WARNING_LED = 13;

/* --- Battery Monitoring --------------------------------------------------- */

/** @brief Battery voltage divider analog input (ADC). */
constexpr uint8_t BATTERY_SENSE = A0;

} // namespace Pins

/* ============================================================================
 * ENCODER SPECIFICATIONS
 * ============================================================================ */

namespace EncoderSpec {

/** @brief Pulses Per Revolution of the wheel encoder. */
constexpr double PPR = 385.0;

/** @brief Radians per encoder pulse. */
constexpr double RADIANS_PER_PULSE = (2.0 * 3.14159265359) / PPR;

/** @brief Conversion factor: RPM to rad/s. */
constexpr double RPM_TO_RADS = (2.0 * 3.14159265359) / 60.0;

} // namespace EncoderSpec

/* ============================================================================
 * ROBOT PHYSICAL PARAMETERS
 * ============================================================================ */

namespace RobotParams {

/** @brief Wheel diameter in millimeters. */
constexpr double WHEEL_DIAMETER_MM = 65.0;

/** @brief Wheel radius in meters. */
constexpr double WHEEL_RADIUS_M = (WHEEL_DIAMETER_MM / 1000.0) / 2.0;

/** @brief Center-to-center distance between left and right wheels (mm). */
constexpr double WHEEL_BASE_MM = 150.0;

/** @brief Wheel base in meters. */
constexpr double WHEEL_BASE_M = WHEEL_BASE_MM / 1000.0;

/** @brief Encoder ticks per meter of wheel travel. */
constexpr double TICKS_PER_METER =
    EncoderSpec::PPR / (3.14159265359 * WHEEL_DIAMETER_MM / 1000.0);

} // namespace RobotParams

/* ============================================================================
 * CONTROL LOOP TIMING
 * ============================================================================ */

namespace Timing {

/** @brief Main PID control loop period in milliseconds (10 Hz). */
constexpr unsigned long CONTROL_INTERVAL_MS = 100;

/** @brief Control interval in seconds (for calculations). */
constexpr double CONTROL_INTERVAL_S = CONTROL_INTERVAL_MS / 1000.0;

/** @brief Velocity telemetry output rate in milliseconds (10 Hz). */
constexpr unsigned long TELEMETRY_INTERVAL_MS = 100;

/** @brief IMU read interval in milliseconds (50 Hz). */
constexpr unsigned long IMU_INTERVAL_MS = 20;

/** @brief Sensor read interval in milliseconds (20 Hz). */
constexpr unsigned long SENSOR_INTERVAL_MS = 50;

/** @brief Lift controller update interval in milliseconds (20 Hz). */
constexpr unsigned long LIFT_INTERVAL_MS = 50;

} // namespace Timing

/* ============================================================================
 * PID TUNING PARAMETERS
 * ============================================================================ */

namespace PIDTuning {

/* --- Right Motor PID Gains --- */
constexpr double KP_RIGHT = 11.5;  ///< Proportional gain
constexpr double KI_RIGHT = 7.5;   ///< Integral gain
constexpr double KD_RIGHT = 0.1;   ///< Derivative gain

/* --- Left Motor PID Gains --- */
constexpr double KP_LEFT = 12.8;   ///< Proportional gain
constexpr double KI_LEFT = 8.3;    ///< Integral gain
constexpr double KD_LEFT = 0.1;    ///< Derivative gain

/* --- Output Limits --- */
constexpr double OUTPUT_MIN = -255.0;  ///< Full reverse PWM
constexpr double OUTPUT_MAX = 255.0;   ///< Full forward PWM

/* --- Anti-Windup --- */
constexpr double INTEGRAL_WINDUP_LIMIT = 200.0;  ///< Max integral accumulation

} // namespace PIDTuning

/* ============================================================================
 * MOTION CONSTRAINTS
 * ============================================================================ */

namespace MotionLimits {

/** @brief Maximum acceleration rate (rad/s per second). */
constexpr double MAX_ACCELERATION = 15.0;

/** @brief Maximum deceleration rate (rad/s per second). */
constexpr double MAX_DECELERATION = 20.0;

/** @brief Minimum PWM output to overcome motor stiction. */
constexpr double MIN_OUTPUT_DEADBAND = 8.0;

/** @brief Velocity threshold below which wheel is considered stopped (rad/s). */
constexpr double VELOCITY_TOLERANCE = 0.01;

} // namespace MotionLimits

/* ============================================================================
 * SAFETY PARAMETERS
 * ============================================================================ */

namespace SafetyConfig {

/** @brief Serial command timeout before E-Stop (milliseconds). */
constexpr unsigned long COMMAND_TIMEOUT_MS = 1500;

/** @brief Hardware watchdog timeout (milliseconds). */
constexpr unsigned long WATCHDOG_TIMEOUT_MS = 2000;

/** @brief Maximum allowed wheel velocity (rad/s). Commands above this are clamped. */
constexpr double MAX_VELOCITY_RAD_S = 10.0;

/** @brief Low battery voltage threshold (millivolts). */
constexpr uint16_t BATTERY_VOLTAGE_LOW = 10800;

/** @brief High battery voltage threshold (millivolts). */
constexpr uint16_t BATTERY_VOLTAGE_HIGH = 16800;

/** @brief Voltage divider ratio (R1+R2) / R2 for battery sensing. */
constexpr double VOLTAGE_DIVIDER_RATIO = 3.3;

} // namespace SafetyConfig

/* ============================================================================
 * LIFT MECHANISM CONFIGURATION
 * ============================================================================ */

namespace LiftConfig {

/** @brief Lift motor PWM speed (0-255). */
constexpr uint8_t LIFT_PWM_SPEED = 200;

/** @brief Maximum time for a lift travel operation before stall fault (ms). */
constexpr unsigned long STALL_TIMEOUT_MS = 5000;

/** @brief Limit switch debounce period (ms). */
constexpr unsigned long DEBOUNCE_MS = 50;

/** @brief Number of electromagnet channels. */
constexpr uint8_t NUM_MAGNETS = 5;

} // namespace LiftConfig

/* ============================================================================
 * SENSOR CONFIGURATION
 * ============================================================================ */

namespace SensorConfig {

/** @brief Proximity sensor ADC threshold for obstacle detection. */
constexpr uint16_t PROXIMITY_THRESHOLD = 500;

/** @brief Number of proximity sensors. */
constexpr uint8_t NUM_PROXIMITY = 5;

/** @brief Metal detector input debounce period (ms). */
constexpr unsigned long METAL_DEBOUNCE_MS = 100;

/** @brief Number of ADC samples to average per proximity read. */
constexpr uint8_t ADC_SAMPLES = 4;

/** @brief Buzzer tone frequency in Hz for mine detection alert. */
constexpr uint16_t BUZZER_FREQ_HZ = 500;

} // namespace SensorConfig

/* ============================================================================
 * IMU CONFIGURATION (MPU6050)
 * ============================================================================ */

namespace IMUConfig {

/** @brief MPU6050 I2C address (AD0 pin = LOW). */
constexpr uint8_t MPU6050_ADDR = 0x68;

/** @brief WHO_AM_I register expected value for MPU6050. */
constexpr uint8_t WHO_AM_I_EXPECTED = 0x68;

/** @brief Number of samples for gyroscope bias calibration at startup. */
constexpr uint16_t CALIBRATION_SAMPLES = 500;

/** @brief Complementary filter coefficient (0.0 = pure accel, 1.0 = pure gyro). */
constexpr double COMPLEMENTARY_ALPHA = 0.98;

/** @brief Gyroscope sensitivity scale factor (LSB per deg/s for +/-250 dps). */
constexpr double GYRO_SCALE = 131.0;

/** @brief Accelerometer sensitivity scale factor (LSB per g for +/-2g). */
constexpr double ACCEL_SCALE = 16384.0;

} // namespace IMUConfig

/* ============================================================================
 * SERIAL COMMUNICATION
 * ============================================================================ */

namespace SerialConfig {

/** @brief USB serial baud rate. */
constexpr unsigned long BAUD_RATE = 115200;

/** @brief Receive buffer size (bytes, stack-allocated). */
constexpr size_t RX_BUFFER_SIZE = 32;

/** @brief Transmit formatting buffer size (bytes, stack-allocated). */
constexpr size_t TX_BUFFER_SIZE = 64;

/** @brief Extended command buffer size (bytes). */
constexpr size_t CMD_BUFFER_SIZE = 24;

/** @brief Packet delimiter character. */
constexpr char PACKET_DELIMITER = '\n';

} // namespace SerialConfig

/* ============================================================================
 * DIAGNOSTICS SETTINGS
 * ============================================================================ */

namespace DiagConfig {

/** @brief Interval between periodic status reports (ms). */
constexpr unsigned long STATUS_INTERVAL_MS = 5000;

/** @brief Enable verbose encoder debug output. */
constexpr bool VERBOSE_ENCODER = false;

/** @brief Enable verbose PID debug output. */
constexpr bool VERBOSE_PID = false;

/** @brief Enable verbose safety state change output. */
constexpr bool VERBOSE_SAFETY = true;

} // namespace DiagConfig

#endif // CONFIG_H