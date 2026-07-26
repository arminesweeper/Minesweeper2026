/**
 * ============================================================================
 * CONFIG.H - System Configuration & Hardware Definitions
 * ============================================================================
 * Central configuration file for the Minesweeper Robot Motor Controller.
 * Defines pinouts, compile-time flags, and default EEPROM parameters.
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

#define ENABLE_WATCHDOG         1
#define ENABLE_ODOMETRY         1
#define ENABLE_DIAGNOSTICS      1
#define ENABLE_SAFETY_MONITOR   1
#define ENABLE_IMU              1
#define ENABLE_LIFT             1
#define ENABLE_SENSORS          1

/* ============================================================================
 * HARDWARE PIN DEFINITIONS
 * ============================================================================ */

namespace Pins {
    /* Motor Driver */
    constexpr uint8_t MOTOR_R_PWM = 9;
    constexpr uint8_t MOTOR_R_DIR = 12;
    constexpr uint8_t MOTOR_L_PWM = 11;
    constexpr uint8_t MOTOR_L_DIR = 7;

    /* Encoders */
    constexpr uint8_t ENCODER_R_A = 3; // INT5
    constexpr uint8_t ENCODER_R_B = 5;
    constexpr uint8_t ENCODER_L_A = 2; // INT4
    constexpr uint8_t ENCODER_L_B = 4;

    /* IMU */
    constexpr uint8_t IMU_SDA = 20;
    constexpr uint8_t IMU_SCL = 21;

    /* Lift */
    constexpr uint8_t LIFT_PWM = 10;
    constexpr uint8_t LIFT_DIR = 8;
    constexpr uint8_t LIMIT_SW_TOP = 28;
    constexpr uint8_t LIMIT_SW_BOTTOM = 29;

    /* Electromagnets */
    constexpr uint8_t MAGNET_PINS[] = {22, 23, 24, 25, 26};

    /* Sensors */
    constexpr uint8_t METAL_DETECTOR = 27;
    constexpr uint8_t PROXIMITY_PINS[] = {A1, A2, A3, A4, A5};

    /* Indicators & Battery */
    constexpr uint8_t BUZZER = 6;
    constexpr uint8_t WARNING_LED = 13;
    constexpr uint8_t BATTERY_SENSE = A0;
}

/* ============================================================================
 * FIRMWARE CONSTANTS (Non-configurable runtime bounds)
 * ============================================================================ */

namespace SafetyConfig {
    constexpr unsigned long COMMAND_TIMEOUT_MS = 1500;
    constexpr unsigned long WATCHDOG_TIMEOUT_MS = 2000;
    constexpr uint16_t BATTERY_VOLTAGE_LOW = 10800; // mV
    constexpr uint16_t BATTERY_VOLTAGE_CRITICAL = 9600; // mV
    constexpr uint16_t BATTERY_VOLTAGE_HIGH = 16800; // mV
    constexpr float VOLTAGE_DIVIDER_RATIO = 3.3f;
}

namespace LiftConfig {
    constexpr uint8_t LIFT_PWM_SPEED = 200;
    constexpr unsigned long STALL_TIMEOUT_MS = 5000;
    constexpr unsigned long DEBOUNCE_MS = 50;
    constexpr uint8_t NUM_MAGNETS = 5;
}

namespace SensorConfig {
    constexpr uint16_t PROXIMITY_THRESHOLD = 500;
    constexpr uint8_t NUM_PROXIMITY = 5;
    constexpr unsigned long METAL_DEBOUNCE_MS = 100;
    constexpr uint8_t ADC_SAMPLES = 4;
    constexpr uint16_t BUZZER_FREQ_HZ = 500;
}

namespace IMUConfig {
    constexpr uint8_t MPU6050_ADDR = 0x68;
    constexpr uint8_t WHO_AM_I_EXPECTED = 0x68;
    constexpr float COMPLEMENTARY_ALPHA = 0.98f;
    constexpr float GYRO_SCALE = 131.0f;
    constexpr float ACCEL_SCALE = 16384.0f;
}

namespace SerialConfig {
    constexpr unsigned long BAUD_RATE = 115200;
    constexpr size_t RX_BUFFER_SIZE = 32;
    constexpr size_t TX_BUFFER_SIZE = 64;
    constexpr size_t CMD_BUFFER_SIZE = 24;
}

/* ============================================================================
 * ROBOT PARAMETERS (EEPROM-Backed)
 * ============================================================================ */

/**
 * @brief Robot parameters structure stored in EEPROM.
 * Provides all tuning and kinematic data for the system.
 */
struct RobotParameters {
    uint32_t magic_number; // Used to verify EEPROM validity
    uint16_t version;

    // Kinematics
    float wheel_radius_m;
    float wheel_base_m;
    float encoder_ppr;
    float rad_per_pulse;

    // Right PID
    float kp_right;
    float ki_right;
    float kd_right;
    float ff_right; // Feedforward

    // Left PID
    float kp_left;
    float ki_left;
    float kd_left;
    float ff_left;

    // Motion Limits
    float max_velocity;
    float max_acceleration;
    float max_deceleration;
    float min_pwm_deadband; // Minimum PWM to overcome stiction
    float max_pwm_slew_rate; // Max change in PWM per second

    // IMU Calibration
    float gyro_bias_x;
    float gyro_bias_y;
    float gyro_bias_z;

    // Hardware configuration flags
    bool invert_right_encoder;
    bool invert_left_encoder;
    bool invert_right_motor;
    bool invert_left_motor;

    uint16_t crc16; // Data integrity check
};

namespace DefaultParams {
    constexpr uint32_t MAGIC_NUMBER = 0x4D494E45; // "MINE"
    constexpr uint16_t VERSION = 1;

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
    constexpr float MAX_PWM_SLEW_RATE = 1000.0f; // 1000 PWM steps per second

    constexpr float GYRO_BIAS_X = 0.0f;
    constexpr float GYRO_BIAS_Y = 0.0f;
    constexpr float GYRO_BIAS_Z = 0.0f;

    constexpr bool INVERT_RIGHT_ENCODER = false;
    constexpr bool INVERT_LEFT_ENCODER = true;
    constexpr bool INVERT_RIGHT_MOTOR = false;
    constexpr bool INVERT_LEFT_MOTOR = true;
}

#endif // CONFIG_H