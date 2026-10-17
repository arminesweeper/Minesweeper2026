/**
 * ============================================================================
 * CONFIG.H - System Configuration & Hardware Definitions
 * ============================================================================
 * Synchronized to as-fabricated Robotics_Mine shield PCB (no PCB edits).
 *
 * Hardware authority:
 *   1. Copper on the Mega shield footprint (use as-is)
 *   2. External jumper / flying-lead harness for unrouted headers
 *   3. Soft-I2C IMU harness (avoids D20/D21 encoder conflict without cutting copper)
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
 * ============================================================================
 * Use these macros to enable or disable individual subsystems at build time.
 * Setting a flag to 0 strips the task execution and reduces CPU/SRAM load.
 * ============================================================================
 */

/**
 * @brief Simple Serial Monitor testing mode.
 * 1 = Run standalone open-loop motor and sensor test routines via Serial Monitor.
 * 0 = Normal operational mode (autonomous / teleoperation with ROS/WiFi).
 */
#define TEST_MODE 0

/**
 * @brief Hardware AVR Watchdog Timer (WDT).
 * 1 = Enable 2-second AVR hardware watchdog; reboots MCU if loop hangs.
 * 0 = Disable hardware watchdog (recommended during debugging via serial).
 */
#define ENABLE_WATCHDOG 0

/**
 * @brief Dead-reckoning wheel odometry calculation.
 * 1 = Compute X, Y, Theta from wheel encoder ticks at 50 Hz and report via serial.
 * 0 = Disable odometry calculations to save CPU cycles.
 */
#define ENABLE_ODOMETRY 0

/**
 * @brief System performance and diagnostics tracking.
 * 1 = Monitor loop frequency, execution latency, free SRAM, and fault counters.
 * 0 = Disable diagnostics tracking for maximum loop throughput.
 */
#define ENABLE_DIAGNOSTICS 0

/**
 * @brief Active safety and fault monitoring system.
 * 1 = Monitor command timeouts, battery thresholds, runaway conditions, and e-stop.
 * 0 = Bypass safety monitor state machine (forces robot state to RUNNING).
 */
#define ENABLE_SAFETY_MONITOR 1

/**
 * @brief 6-DOF IMU attitude and motion tracking.
 * 1 = Sample MPU6050 accelerometer and gyroscope, compute pitch/roll/yaw.
 * 0 = Disable IMU task (useful if IMU harness is not connected).
 */
#define ENABLE_IMU 0

/**
 * @brief Automated mine lift mechanism & electromagnets.
 * 1 = Enable lift motor controller, upper/lower limit switches, and magnet cycle.
 * 0 = Disable lift subsystem.
 */
#define ENABLE_LIFT 1

/**
 * @brief Environmental sensing (5x Proximity sensors & Ultrasonic).
 * 1 = Periodically sample proximity array and ultrasonic sensor.
 * 0 = Disable sensor sampling.
 */
#define ENABLE_SENSORS 1

/**
 * @brief Pulse induction metal detector input (Pin 33).
 * 1 = Sample digital metal detector input on Pin 33.
 * 0 = Disable metal detector (treated as always false / disconnected for debugging).
 */
#define ENABLE_METAL_DETECTOR 1

/**
 * @brief Auxiliary hardware serial port (Serial2 on Pins 16/17).
 * 1 = Enable Serial2 communication for ESP8266 / WiFi Web Controller.
 * 0 = Disable Serial2 (saves RX/TX buffers and interrupts when only USB is used).
 */
#define ENABLE_SERIAL2 0

/**
 * @brief Closed-loop PID velocity control.
 * 1 = Enable closed-loop PID controllers with encoder velocity feedback.
 * 0 = Switch to open-loop direct drive (maps target speed directly to motor PWM).
 */
#define ENABLE_PID 1

/**
 * @brief MPU6050 I2C interface mode selection.
 * Shield copper routes Encoder 1 to D20/D21 (hardware I2C pins).
 * 1 = Use software bit-banged I2C on dedicated pins (Pins::IMU_SDA, Pins::IMU_SCL).
 * 0 = Use hardware Wire on D20/D21 (only if Encoder 1 is physically disconnected).
 */
#define IMU_USE_SOFT_I2C 1

/**
 * @brief Electromagnet PCB relay routing configuration.
 * PCB drives all 5 electromagnet headers through a single shared relay (D38).
 * 1 = Single relay drives all magnets simultaneously (standard PCB configuration).
 * 0 = Discrete GPIO per magnet (requires external multi-channel driver board).
 */
#define MAGNET_SHARED_RELAY 1

/**
 * @brief Siren output driver type.
 * PCB routes Pin 47 to a 12V automotive siren relay (J15).
 * 1 = Digital ON/OFF trigger for 12V relay-driven siren (no PWM tone generation).
 * 0 = Piezo speaker output using Arduino tone() / noTone() frequency generation.
 */
#define BUZZER_IS_SIREN_RELAY 1

/**
 * @brief Siren relay activation electrical logic level.
 * 1 = Active-HIGH: logic HIGH turns siren ON, LOW turns siren OFF.
 * 0 = Active-LOW: logic LOW turns siren ON, HIGH turns siren OFF (standard for optoisolated relay boards).
 */
#define SIREN_ACTIVE_HIGH 0

/**
 * @brief Limit switch electrical logic level.
 * 1 = Active-HIGH: switch closure delivers logic HIGH when powered by the magnet relay, open delivers 0V (LOW).
 * 0 = Active-LOW: switch closure pulls pin to GND (LOW).
 */
#define LIMIT_SWITCH_ACTIVE_HIGH 1

/**
 * @brief Gripper mine detection via ultrasonic sensor.
 * 1 = Use HC-SR04 ultrasonic distance sensor (Pins 43 & 45) to detect mine catch.
 * 0 = Manual trigger / command only.
 */
#define USE_ULTRASONIC_OBJECT_DETECT 1

/* ============================================================================
 * HARDWARE PIN DEFINITIONS — Synchronized with Physical Robot Wiring
 * ============================================================================
 * Hardware pinout matching physical robot (Safi's configuration):
 *   - Drive Motors: Right (PWM 44, DIR 42), Left (PWM 46, DIR 40)
 *   - Encoders: Right (INT2 D19, D18), Left (INT0 D21, D20)
 *   - Lift Actuator: PWM 7 (Timer 4 OC4B), DIR 49
 *   - Limit Switches: Top 53, Bottom 51 (Active-HIGH)
 *   - Ultrasonic Sensor: Trig 43, Echo 45 (HC-SR04)
 *   - Proximity Array: Digital inputs D23, D25, D27, D29, D31
 *   - Metal Detector: D33 (Active-HIGH)
 *   - Siren Relay: D37 (12V automotive siren relay)
 *   - Shared Magnet Relay: D38
 *   - Camera Servo: D4 (Timer 3 custom 50 Hz PWM, preserved per user request)
 * ============================================================================
 */

namespace Pins {
/* --- Primary Drive Motors (Cytron MDD10A via J36, PCB copper) --- */
constexpr uint8_t MOTOR_R_PWM = 44; /**< Right motor speed PWM (Timer 5, OC5C) */
constexpr uint8_t MOTOR_R_DIR = 42; /**< Right motor direction pin (DIR1) */
constexpr uint8_t MOTOR_L_PWM = 46; /**< Left motor speed PWM (Timer 5, OC5A) */
constexpr uint8_t MOTOR_L_DIR = 40; /**< Left motor direction pin (DIR2) */

/* --- Primary Wheel Quadrature Encoders (PCB copper) --- */
constexpr uint8_t ENCODER_R_A = 19; /**< Right wheel encoder phase A (External Interrupt INT2) */
constexpr uint8_t ENCODER_R_B = 18; /**< Right wheel encoder phase B (Direction sensing) */
constexpr uint8_t ENCODER_L_A = 21; /**< Left wheel encoder phase A (External Interrupt INT0) */
constexpr uint8_t ENCODER_L_B = 20; /**< Left wheel encoder phase B (Direction sensing) */

/* --- MPU6050 IMU (Software bit-bang I2C on dedicated flying leads) --- */
constexpr uint8_t IMU_SDA = 34; /**< Soft-I2C Serial Data line (moved to D34 to avoid conflict with Prox 5) */
constexpr uint8_t IMU_SCL = 36; /**< Soft-I2C Serial Clock line (moved to D36 to avoid conflict with Metal Detector) */

/* --- Lift Mechanism & Travel Limit Switches --- */
constexpr uint8_t LIFT_PWM = 7;         /**< Lift actuator motor speed PWM (Timer 4 OC4B, hardware PWM) */
constexpr uint8_t LIFT_DIR = 49;        /**< Lift actuator motor direction pin */
constexpr uint8_t LIMIT_SW_TOP = 53;    /**< Upper travel limit switch (Active HIGH, D53) */
constexpr uint8_t LIMIT_SW_BOTTOM = 51; /**< Lower travel limit switch (Active HIGH, D51) */

/* --- HC-SR04 Ultrasonic Distance Sensor (Gripper mine detection) --- */
constexpr uint8_t US_TRIG = 43;         /**< Ultrasonic trigger pulse output */
constexpr uint8_t US_ECHO = 45;         /**< Ultrasonic echo pulse input */

/* --- Electromagnet Relay Outputs --- */
#if MAGNET_SHARED_RELAY
constexpr uint8_t MAGNET_RELAY = 38;                   /**< Single shared relay control pin for all magnets */
constexpr uint8_t MAGNET_PINS[] = {38, 38, 38, 38, 38};/**< Mapped channels all resolve to shared pin 38 */
#else
constexpr uint8_t MAGNET_PINS[] = {22, 24, 26, 28, 30};/**< Discrete individual pins for 5 magnet drivers */
#endif

/* --- Sensor Array Inputs --- */
constexpr uint8_t METAL_DETECTOR = 33;                       /**< Pulse induction metal detector digital input (Active HIGH) */
constexpr uint8_t PROXIMITY_PINS[] = {23, 25, 27, 29, 31};   /**< 5x Digital proximity sensor inputs (D23, D25, D27, D29, D31) */

/* --- Status & Indicator Outputs --- */
constexpr uint8_t BUZZER = 37;        /**< 12V high-output siren relay control pin */
constexpr uint8_t WARNING_LED = 13;   /**< Diagnostic / warning LED output (onboard LED_BUILTIN; avoids D45 clash) */
constexpr uint8_t BATTERY_SENSE = A0; /**< Scaled main battery voltage divider analog input */

/* --- Camera Pan Servo (Timer 3 Custom 50 Hz PWM) --- */
constexpr uint8_t SERVO = 4; /**< Camera pan servo signal pin (Timer 3, preserved on Pin 4) */

/* --- Secondary 4WD Drive Motors (Cytron 2 via J37, optional 4WD expansion) --- */
constexpr uint8_t MOTOR_R_PWM_2 = 9;  /**< Secondary right motor PWM (Timer 2, OC2B) */
constexpr uint8_t MOTOR_R_DIR_2 = 8;  /**< Secondary right motor direction pin */
constexpr uint8_t MOTOR_L_PWM_2 = 11; /**< Secondary left motor PWM (Timer 1, OC1A) */
constexpr uint8_t MOTOR_L_DIR_2 = 10; /**< Secondary left motor direction pin */
} // namespace Pins

/* ============================================================================
 * TIMING CONFIGURATION (Cooperative Scheduler Task Periods)
 * ============================================================================
 * Defines execution rate for non-blocking cooperative tasks in SystemManager.
 * ============================================================================
 */

namespace Timing {
constexpr unsigned long CONTROL_INTERVAL_MS = 20;   /**< Velocity PID control loop period (20 ms = 50 Hz) */
constexpr unsigned long IMU_INTERVAL_MS = 20;       /**< IMU sampling & attitude filter period (20 ms = 50 Hz) */
constexpr unsigned long SENSOR_INTERVAL_MS = 50;    /**< Metal detector & proximity ADC scan period (50 ms = 20 Hz) */
constexpr unsigned long LIFT_INTERVAL_MS = 50;      /**< Automated lift state machine update period (50 ms = 20 Hz) */
constexpr unsigned long TELEMETRY_INTERVAL_MS = 100;/**< High-rate serial telemetry period (100 ms = 10 Hz) */
} // namespace Timing

/* ============================================================================
 * PID TUNING DEFAULTS (Factory baseline; EEPROM overrides at runtime)
 * ============================================================================
 * Discrete PID velocity controller gains for left and right wheels.
 * ============================================================================
 */

namespace PIDTuning {
constexpr float KP_RIGHT = 11.5f; /**< Right wheel Proportional gain (PWM per rad/s error) */
constexpr float KI_RIGHT = 7.5f;  /**< Right wheel Integral gain (PWM per rad/s-second error) */
constexpr float KD_RIGHT = 0.1f;  /**< Right wheel Derivative gain (dampens overshoot) */

constexpr float KP_LEFT = 12.8f;  /**< Left wheel Proportional gain (PWM per rad/s error) */
constexpr float KI_LEFT = 8.3f;   /**< Left wheel Integral gain (PWM per rad/s-second error) */
constexpr float KD_LEFT = 0.1f;   /**< Left wheel Derivative gain (dampens overshoot) */

constexpr float OUTPUT_MIN = -255.0f;          /**< Minimum PID output limit (full reverse PWM) */
constexpr float OUTPUT_MAX = 255.0f;           /**< Maximum PID output limit (full forward PWM) */
constexpr float INTEGRAL_WINDUP_LIMIT = 200.0f;/**< Absolute limit for accumulated integral term */
} // namespace PIDTuning

/* ============================================================================
 * SAFETY CONFIGURATION (Thresholds & Timeouts)
 * ============================================================================
 * Electrical and communication safety limits to protect robot hardware.
 * ============================================================================
 */

namespace SafetyConfig {
constexpr unsigned long COMMAND_TIMEOUT_MS = 1500;  /**< Max time between ROS drive packets before auto-stop (ms) */
constexpr unsigned long WATCHDOG_TIMEOUT_MS = 2000; /**< Maximum loop stall time before watchdog triggers (ms) */
constexpr uint16_t BATTERY_VOLTAGE_LOW = 10800;     /**< Low battery warning threshold (10.8 V, 3.6V/cell 3S LiPo) */
constexpr uint16_t BATTERY_VOLTAGE_CRITICAL = 9600; /**< Critical battery e-stop threshold (9.6 V, 3.2V/cell 3S LiPo) */
constexpr uint16_t BATTERY_VOLTAGE_HIGH = 16800;    /**< Overvoltage protection threshold (16.8 V, max 4S LiPo) */
constexpr float VOLTAGE_DIVIDER_RATIO = 3.3f;       /**< Resistor divider attenuation factor on A0 input */
} // namespace SafetyConfig

/* ============================================================================
 * LIFT MECHANISM CONFIGURATION
 * ============================================================================
 * Motor speeds, timeouts, and dwell times for the automated mine retriever.
 * ============================================================================
 */

namespace LiftConfig {
constexpr uint8_t LIFT_PWM_SPEED = 200;            /**< Actuator motor drive speed (0-255 PWM) */
constexpr unsigned long STALL_TIMEOUT_MS = 8000;   /**< Max motor run duration before aborting as stalled (ms) */
constexpr unsigned long DEBOUNCE_MS = 20;          /**< Limit switch software debounce duration (ms) */
constexpr unsigned long DROP_DWELL_MS = 300;       /**< Dwell time at top position with magnet OFF to drop mine (ms) */
constexpr unsigned long MAGNET_ARM_TIMEOUT_MS = 5000;/**< Max time magnet stays armed in IDLE if mine not caught (prevents heating) */
constexpr uint8_t NUM_MAGNETS = 5;                 /**< Number of virtual/physical electromagnet channels */
constexpr bool LIFT_INVERT_DIR = false;            /**< Invert lift motor direction if UP/DOWN are swapped */
} // namespace LiftConfig

/* ============================================================================
 * SENSOR ARRAY CONFIGURATION
 * ============================================================================
 * Thresholds and sampling parameters for mine detection and obstacle avoidance.
 * ============================================================================
 */

namespace SensorConfig {
constexpr uint16_t PROXIMITY_THRESHOLD = 500;    /**< Threshold (0-1023) for proximity obstacle trigger */
constexpr uint8_t NUM_PROXIMITY = 5;             /**< Total number of digital proximity sensors connected */
constexpr uint8_t PROXIMITY_ACTIVE_STATE = LOW;  /**< Sensor logic level indicating obstacle detected (LOW for active-low IR modules) */
constexpr unsigned long METAL_DEBOUNCE_MS = 50;  /**< Minimum trigger confirmation time for metal detector (ms) */
constexpr uint8_t METAL_ACTIVE_STATE = HIGH;     /**< Sensor logic level indicating metal is detected */
constexpr uint8_t ADC_SAMPLES = 1;               /**< Number of ADC conversions per read (1 = fast non-blocking) */
constexpr uint16_t BUZZER_FREQ_HZ = 500;         /**< Audio tone frequency in Hz (used only if piezo is fitted) */

/* --- HC-SR04 Ultrasonic Distance Sensor Configuration --- */
constexpr uint16_t US_MAX_DISTANCE_CM = 250;     /**< Maximum measurement range cap in cm (250 cm) */
constexpr uint16_t US_OBJECT_DETECT_CM = 15;     /**< Distance threshold under gripper to consider mine caught (<= 15 cm) */
constexpr uint16_t US_START_TIMEOUT_US = 1000;   /**< Timeout waiting for echo pulse to start (1000 us = 1 ms) */
constexpr uint16_t US_MAX_ECHO_US = 6000;        /**< Timeout waiting for echo pulse to end (6000 us ≈ 100 cm range) */
} // namespace SensorConfig

/* ============================================================================
 * IMU (MPU6050) CONFIGURATION
 * ============================================================================
 * Registers, scaling factors, and filter weights for inertial measurement.
 * ============================================================================
 */

namespace IMUConfig {
constexpr uint8_t MPU6050_ADDR = 0x68;          /**< MPU6050 7-bit I2C slave address (AD0 low) */
constexpr uint8_t WHO_AM_I_EXPECTED = 0x68;     /**< Expected WHO_AM_I register (0x75) response */
constexpr float COMPLEMENTARY_ALPHA = 0.98f;    /**< Complementary filter gyro weight factor (alpha) */
constexpr float GYRO_SCALE = 131.0f;            /**< Gyro sensitivity divider for ±250 deg/s range (LSB/(deg/s)) */
constexpr float ACCEL_SCALE = 16384.0f;         /**< Accelerometer sensitivity divider for ±2g range (LSB/g) */
} // namespace IMUConfig

/* ============================================================================
 * SERIAL PROTOCOL CONFIGURATION
 * ============================================================================
 * Baud rates and fixed buffer capacities for USB and auxiliary serial streams.
 * ============================================================================
 */

namespace SerialConfig {
constexpr unsigned long BAUD_RATE = 115200; /**< Serial communication baud rate (115200 bps) */
constexpr size_t RX_BUFFER_SIZE = 32;       /**< Maximum byte length of inbound command buffer */
constexpr size_t TX_BUFFER_SIZE = 64;       /**< Maximum byte length of outbound telemetry buffer */
constexpr size_t CMD_BUFFER_SIZE = 24;      /**< Maximum byte length of extended 'C' command string */
} // namespace SerialConfig

/* ============================================================================
 * ROBOT PARAMETERS (EEPROM-Backed Storage Structure)
 * ============================================================================
 * Non-volatile runtime parameters loaded from EEPROM, overridable over serial.
 * ============================================================================
 */

struct RobotParameters {
  uint32_t magic_number; /**< Validation signature (0x4D494E45) ensuring EEPROM is formatted */
  uint16_t version;      /**< Configuration structure schema revision */

  /* Wheel Kinematics */
  float wheel_radius_m;  /**< Drive wheel radius in meters (default 0.0325 m = 65 mm diam) */
  float wheel_base_m;    /**< Distance between left and right wheel track centers in meters (0.150 m) */
  float encoder_ppr;     /**< Quadrature encoder pulses per single wheel rotation (385.0 PPR) */
  float rad_per_pulse;   /**< Angular radians traveled per single encoder pulse ((2*PI)/PPR) */

  /* Right Wheel PID & Feedforward Gains */
  float kp_right; /**< Right wheel proportional gain */
  float ki_right; /**< Right wheel integral gain */
  float kd_right; /**< Right wheel derivative gain */
  float ff_right; /**< Right wheel feedforward gain */

  /* Left Wheel PID & Feedforward Gains */
  float kp_left;  /**< Left wheel proportional gain */
  float ki_left;  /**< Left wheel integral gain */
  float kd_left;  /**< Left wheel derivative gain */
  float ff_left;  /**< Left wheel feedforward gain */

  /* Velocity & Acceleration Limits */
  float max_velocity;      /**< Maximum allowable wheel angular speed in rad/s (10.0 rad/s) */
  float max_acceleration;  /**< Maximum velocity ramp acceleration in rad/s² (15.0 rad/s²) */
  float max_deceleration;  /**< Maximum velocity ramp braking deceleration in rad/s² (20.0 rad/s²) */
  float min_pwm_deadband;  /**< Minimum motor PWM threshold to overcome static friction (35.0) */
  float max_pwm_slew_rate; /**< Maximum motor PWM step change rate per second (1000.0 PWM/s) */

  /* IMU Calibration Biases */
  float gyro_bias_x; /**< Gyroscope X-axis zero-rate offset in deg/s */
  float gyro_bias_y; /**< Gyroscope Y-axis zero-rate offset in deg/s */
  float gyro_bias_z; /**< Gyroscope Z-axis zero-rate offset in deg/s */

  /* Polarity & Direction Flags */
  bool invert_right_encoder; /**< Invert right encoder counting direction */
  bool invert_left_encoder;  /**< Invert left encoder counting direction */
  bool invert_right_motor;   /**< Invert right motor rotation direction */
  bool invert_left_motor;    /**< Invert left motor rotation direction */

  uint16_t crc16; /**< 16-bit CRC checksum for validating EEPROM integrity */
};

/* ============================================================================
 * DEFAULT PARAMETERS (Factory defaults used on first boot / EEPROM wipe)
 * ============================================================================
 */

namespace DefaultParams {
constexpr uint32_t MAGIC_NUMBER = 0x4D494E45; /**< "MINE" signature */
constexpr uint16_t VERSION = 5;               /**< Schema version 5 (Calibrated feedforward & deadband) */

constexpr float WHEEL_DIAMETER_MM = 65.0f;                            /**< Outer tire diameter in millimeters */
constexpr float WHEEL_RADIUS_M = (WHEEL_DIAMETER_MM / 1000.0f) / 2.0f;/**< Calibrated wheel radius (0.0325 m) */
constexpr float WHEEL_BASE_M = 150.0f / 1000.0f;                      /**< Track width between drive wheels (0.150 m) */
constexpr float ENCODER_PPR = 385.0f;                                 /**< Measured quadrature pulses per revolution */
constexpr float RAD_PER_PULSE = (2.0f * PI) / ENCODER_PPR;           /**< Radians per pulse ratio */

constexpr float KP_RIGHT = 11.5f; /**< Factory default right wheel P gain */
constexpr float KI_RIGHT = 7.5f;  /**< Factory default right wheel I gain */
constexpr float KD_RIGHT = 0.1f;  /**< Factory default right wheel D gain */
constexpr float FF_RIGHT = 22.0f; /**< Factory default right wheel feedforward gain (22.0 PWM per rad/s) */

constexpr float KP_LEFT = 12.8f;  /**< Factory default left wheel P gain */
constexpr float KI_LEFT = 8.3f;   /**< Factory default left wheel I gain */
constexpr float KD_LEFT = 0.1f;   /**< Factory default left wheel D gain */
constexpr float FF_LEFT = 22.0f;  /**< Factory default left wheel feedforward gain (22.0 PWM per rad/s) */

constexpr float MAX_VELOCITY = 10.0f;         /**< Default max velocity (rad/s) */
constexpr float MAX_ACCELERATION = 15.0f;     /**< Default max acceleration (rad/s²) */
constexpr float MAX_DECELERATION = 20.0f;     /**< Default max deceleration (rad/s²) */
constexpr float MIN_PWM_DEADBAND = 35.0f;     /**< Overcomes motor static friction deadband */
constexpr float MAX_PWM_SLEW_RATE = 1000.0f;  /**< Max PWM change rate per second */

constexpr float GYRO_BIAS_X = 0.0f; /**< Factory default Gyro X bias */
constexpr float GYRO_BIAS_Y = 0.0f; /**< Factory default Gyro Y bias */
constexpr float GYRO_BIAS_Z = 0.0f; /**< Factory default Gyro Z bias */

constexpr bool INVERT_RIGHT_ENCODER = false; /**< Right encoder normal polarity */
constexpr bool INVERT_LEFT_ENCODER = true;   /**< Left encoder reversed polarity */
constexpr bool INVERT_RIGHT_MOTOR = false;   /**< Right motor normal direction */
constexpr bool INVERT_LEFT_MOTOR = true;     /**< Left motor reversed direction */
} // namespace DefaultParams

#endif /* CONFIG_H */
