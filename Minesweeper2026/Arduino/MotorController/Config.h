/**
 * ============================================================================
 * CONFIG.H - System Configuration & Hardware Definitions
 * ============================================================================
 * Synchronized to as-fabricated Robotics_Mine shield PCB (no PCB edits).
 *
 * Hardware authority:
 *   1. Copper on the Mega shield footprint (use as-is)
 *   2. External jumper / flying-lead harness for unrouted headers
 *   3. Soft-I2C IMU harness (avoids D20/D21 encoder conflict without cutting
 * copper)
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
 */

#define TEST_MODE 0 // Set to 1 to enable simple Serial Monitor motor testing

#define ENABLE_WATCHDOG 0
#define ENABLE_ODOMETRY 0
#define ENABLE_DIAGNOSTICS 0
#define ENABLE_SAFETY_MONITOR 0
#define ENABLE_IMU 0
#define ENABLE_LIFT 0
#define ENABLE_SENSORS 0

/**
 * PCB copper puts ENC1 on Mega D20/D21 (same MCU pins as hardware I2C).
 * When 1: bit-bang I2C on IMU_SDA/IMU_SCL (flying leads). Do NOT plug MPU
 * data into shield J29/J38 SDA/SCL pads.
 * When 0: use Wire on D20/D21 (only if ENC1 is disconnected).
 */
#define IMU_USE_SOFT_I2C 1

/**
 * PCB drives all 5 electromagnets through a single relay (D38 / J14).
 * Keep a 5-bit software mask for ROS protocol compatibility; hardware OR-gates
 * onto one pin when MAGNET_SHARED_RELAY is 1.
 */
#define MAGNET_SHARED_RELAY 1

/**
 * PCB uses a 12 V siren relay (J15), not a piezo on a PWM pin.
 * When 1: BUZZER pin is digital on/off (no tone()).
 * When 0: classic tone()/noTone() piezo on BUZZER pin.
 */
#define BUZZER_IS_SIREN_RELAY 1

/**
 * Limit switches on the Magnets/ sub-board are stepped down from 12 V
 * (10k / 3.9k). Closed switch ≈ 3.4 V (logic HIGH). Open ≈ 0 V.
 * When 1: pressed = HIGH. When 0: pressed = LOW (INPUT_PULLUP NO-to-GND).
 */
#define LIMIT_SWITCH_ACTIVE_HIGH 1

/* ============================================================================
 * HARDWARE PIN DEFINITIONS — as-fabricated PCB + harness
 * ============================================================================
 *
 * PCB copper (use as-is — no cuts):
 *   D44/D42 = PWM1/DIR1 (Cytron Ch A / right)
 *   D46/D40 = PWM2/DIR2 (Cytron Ch B / left)
 *   D38     = Magnet relay (J14)
 *   D36     = Servo signal (J13)
 *   D21/D20 = ENC1 A1/B1
 *   D19/D18 = ENC2 A2/B2
 *
 * IMU (soft I2C — flying leads, free Mega pins):
 *   D30 = soft SDA, D31 = soft SCL
 *   Leave shield J29/J38 data pins unused (or power-only).
 *
 * EXTERNAL JUMPERS (harness only — not PCB edits):
 *   D10 → J37/J44 PWM3 (lift PWM)
 *   D8  → J37/J44 DIR3 (lift DIR)
 *   D28 → J44/J43 LM1  (limit 1 / top)
 *   D29 → J44/J43 LM2  (limit 2 / bottom)
 *   D27 → J6.1 MD or J40.1 MTLDTCT
 *   D37 → J15.1 SIREN
 *   A1..A5 → J6 AP1..AP5 (Mega analog header)
 *   A0 → battery divider (add-on)
 */

namespace Pins {
/* Drive motors — Cytron MDD10A via J36 (PCB copper) */
constexpr uint8_t MOTOR_R_PWM = 44; /* PWM1, Timer 5 OC5C */
constexpr uint8_t MOTOR_R_DIR = 42; /* DIR1 */
constexpr uint8_t MOTOR_L_PWM = 46; /* PWM2, Timer 5 OC5A */
constexpr uint8_t MOTOR_L_DIR = 40; /* DIR2 */

/* Encoders — match shield copper exactly */
constexpr uint8_t ENCODER_R_A = 19; /* A2, INT2 */
constexpr uint8_t ENCODER_R_B = 18; /* B2 */
constexpr uint8_t ENCODER_L_A = 21; /* A1, INT2 */
constexpr uint8_t ENCODER_L_B = 20; /* B1 */

/* IMU — soft I2C on free pins (not hardware Wire D20/D21) */
constexpr uint8_t IMU_SDA = 31;
constexpr uint8_t IMU_SCL = 33;

/* Lift motor + limits — jumpered to J37 / J44 / J43 */
constexpr uint8_t LIFT_PWM = 37;        /* PWM3 */
constexpr uint8_t LIFT_DIR = 35;        /* DIR3 */
constexpr uint8_t LIMIT_SW_TOP = 41;    /* LM1 */
constexpr uint8_t LIMIT_SW_BOTTOM = 43; /* LM2 */

/* Electromagnets — single PCB relay on D38; software mask still 5 bits */
#if MAGNET_SHARED_RELAY
constexpr uint8_t MAGNET_RELAY = 38;
constexpr uint8_t MAGNET_PINS[] = {38, 38, 38, 38, 38};
#else
constexpr uint8_t MAGNET_PINS[] = {22, 24, 26, 28, 30};
#endif

/* Sensors — jumpered */
constexpr uint8_t METAL_DETECTOR = 39;
constexpr uint8_t PROXIMITY_PINS[] = {A1, A2, A3, A4, A5};

/* Indicators */
constexpr uint8_t BUZZER = 47; /* Siren relay J15 */
constexpr uint8_t WARNING_LED = 45;
constexpr uint8_t BATTERY_SENSE = A0;

/* Optional / future */
constexpr uint8_t SERVO = 4; /* J13 SERVO signal (PCB copper) */

/* Secondary Drive Motors — Cytron2 */
constexpr uint8_t MOTOR_R_PWM_2 = 9; // Hardware PWM
constexpr uint8_t MOTOR_R_DIR_2 = 8;
constexpr uint8_t MOTOR_L_PWM_2 = 11; // Hardware PWM
constexpr uint8_t MOTOR_L_DIR_2 = 10;
} // namespace Pins

/* ============================================================================
 * TIMING (cooperative scheduler periods)
 * ============================================================================
 */

namespace Timing {
constexpr unsigned long CONTROL_INTERVAL_MS = 100; /* 10 Hz PID */
constexpr unsigned long IMU_INTERVAL_MS = 20;      /* 50 Hz */
constexpr unsigned long SENSOR_INTERVAL_MS = 50;   /* 20 Hz */
constexpr unsigned long LIFT_INTERVAL_MS = 50;     /* 20 Hz */
constexpr unsigned long TELEMETRY_INTERVAL_MS = 100;
} // namespace Timing

/* ============================================================================
 * PID TUNING DEFAULTS (construction-time; EEPROM may override at runtime)
 * ============================================================================
 */

namespace PIDTuning {
constexpr float KP_RIGHT = 11.5f;
constexpr float KI_RIGHT = 7.5f;
constexpr float KD_RIGHT = 0.1f;
constexpr float KP_LEFT = 12.8f;
constexpr float KI_LEFT = 8.3f;
constexpr float KD_LEFT = 0.1f;
constexpr float OUTPUT_MIN = -255.0f;
constexpr float OUTPUT_MAX = 255.0f;
constexpr float INTEGRAL_WINDUP_LIMIT = 200.0f;
} // namespace PIDTuning

/* ============================================================================
 * FIRMWARE CONSTANTS (Non-configurable runtime bounds)
 * ============================================================================
 */

namespace SafetyConfig {
constexpr unsigned long COMMAND_TIMEOUT_MS = 1500;
constexpr unsigned long WATCHDOG_TIMEOUT_MS = 2000;
constexpr uint16_t BATTERY_VOLTAGE_LOW = 10800;     /* mV */
constexpr uint16_t BATTERY_VOLTAGE_CRITICAL = 9600; /* mV */
constexpr uint16_t BATTERY_VOLTAGE_HIGH = 16800;    /* mV */
constexpr float VOLTAGE_DIVIDER_RATIO = 3.3f;
} // namespace SafetyConfig

namespace LiftConfig {
constexpr uint8_t LIFT_PWM_SPEED = 200;
constexpr unsigned long STALL_TIMEOUT_MS = 5000;
constexpr unsigned long DEBOUNCE_MS = 50;
constexpr uint8_t NUM_MAGNETS = 5;
} // namespace LiftConfig

namespace SensorConfig {
constexpr uint16_t PROXIMITY_THRESHOLD = 500;
constexpr uint8_t NUM_PROXIMITY = 5;
constexpr unsigned long METAL_DEBOUNCE_MS = 100;
constexpr uint8_t ADC_SAMPLES = 4;
constexpr uint16_t BUZZER_FREQ_HZ =
    500; /* Used only if BUZZER_IS_SIREN_RELAY == 0 */
} // namespace SensorConfig

namespace IMUConfig {
constexpr uint8_t MPU6050_ADDR = 0x68;
constexpr uint8_t WHO_AM_I_EXPECTED = 0x68;
constexpr float COMPLEMENTARY_ALPHA = 0.98f;
constexpr float GYRO_SCALE = 131.0f;
constexpr float ACCEL_SCALE = 16384.0f;
} // namespace IMUConfig

namespace SerialConfig {
constexpr unsigned long BAUD_RATE = 115200;
constexpr size_t RX_BUFFER_SIZE = 32;
constexpr size_t TX_BUFFER_SIZE = 64;
constexpr size_t CMD_BUFFER_SIZE = 24;
} // namespace SerialConfig

/* ============================================================================
 * ROBOT PARAMETERS (EEPROM-Backed)
 * ============================================================================
 */

struct RobotParameters {
  uint32_t magic_number;
  uint16_t version;

  float wheel_radius_m;
  float wheel_base_m;
  float encoder_ppr;
  float rad_per_pulse;

  float kp_right;
  float ki_right;
  float kd_right;
  float ff_right;

  float kp_left;
  float ki_left;
  float kd_left;
  float ff_left;

  float max_velocity;
  float max_acceleration;
  float max_deceleration;
  float min_pwm_deadband;
  float max_pwm_slew_rate;

  float gyro_bias_x;
  float gyro_bias_y;
  float gyro_bias_z;

  bool invert_right_encoder;
  bool invert_left_encoder;
  bool invert_right_motor;
  bool invert_left_motor;

  uint16_t crc16;
};

namespace DefaultParams {
constexpr uint32_t MAGIC_NUMBER = 0x4D494E45; /* "MINE" */
constexpr uint16_t VERSION = 3; /* No PCB edit: ENC on D20/D21, soft-I2C IMU */

constexpr float WHEEL_DIAMETER_MM = 65.0f;
constexpr float WHEEL_RADIUS_M = (WHEEL_DIAMETER_MM / 1000.0f) / 2.0f;
constexpr float WHEEL_BASE_M = 150.0f / 1000.0f;
constexpr float ENCODER_PPR = 385.0f;
constexpr float RAD_PER_PULSE = (2.0f * PI) / ENCODER_PPR;

constexpr float KP_RIGHT = 11.5f;
constexpr float KI_RIGHT = 7.5f;
constexpr float KD_RIGHT = 0.1f;
constexpr float FF_RIGHT = 0.5f;

constexpr float KP_LEFT = 12.8f;
constexpr float KI_LEFT = 8.3f;
constexpr float KD_LEFT = 0.1f;
constexpr float FF_LEFT = 0.5f;

constexpr float MAX_VELOCITY = 10.0f;
constexpr float MAX_ACCELERATION = 15.0f;
constexpr float MAX_DECELERATION = 20.0f;
constexpr float MIN_PWM_DEADBAND = 20.0f;
constexpr float MAX_PWM_SLEW_RATE = 1000.0f;

constexpr float GYRO_BIAS_X = 0.0f;
constexpr float GYRO_BIAS_Y = 0.0f;
constexpr float GYRO_BIAS_Z = 0.0f;

constexpr bool INVERT_RIGHT_ENCODER = false;
constexpr bool INVERT_LEFT_ENCODER = true;
constexpr bool INVERT_RIGHT_MOTOR = false;
constexpr bool INVERT_LEFT_MOTOR = true;
} // namespace DefaultParams

#endif /* CONFIG_H */
