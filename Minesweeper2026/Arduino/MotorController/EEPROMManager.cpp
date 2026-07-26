/**
 * ============================================================================
 * EEPROMManager.cpp
 * ============================================================================
 */

#include "EEPROMManager.h"
#include <EEPROM.h>

#define EEPROM_START_ADDR 0

RobotParameters EEPROMManager::current_params_;

void EEPROMManager::init() {
    EEPROM.get(EEPROM_START_ADDR, current_params_);

    // Validate magic number and version
    bool isValid = (current_params_.magic_number == DefaultParams::MAGIC_NUMBER) &&
                   (current_params_.version == DefaultParams::VERSION);

    // Validate CRC
    if (isValid) {
        size_t data_size = sizeof(RobotParameters) - sizeof(uint16_t); // Exclude CRC field itself
        uint16_t calculated_crc = calculateCRC16((const uint8_t*)&current_params_, data_size);
        if (calculated_crc != current_params_.crc16) {
            isValid = false;
        }
    }

    if (!isValid) {
        resetToDefaults();
    }
}

const RobotParameters& EEPROMManager::getParams() {
    return current_params_;
}

void EEPROMManager::saveParams(const RobotParameters& params) {
    current_params_ = params;
    
    // Calculate new CRC
    size_t data_size = sizeof(RobotParameters) - sizeof(uint16_t);
    current_params_.crc16 = calculateCRC16((const uint8_t*)&current_params_, data_size);
    
    EEPROM.put(EEPROM_START_ADDR, current_params_);
}

void EEPROMManager::resetToDefaults() {
    loadDefaultParams(current_params_);
    saveParams(current_params_);
}

void EEPROMManager::loadDefaultParams(RobotParameters& params) {
    params.magic_number = DefaultParams::MAGIC_NUMBER;
    params.version = DefaultParams::VERSION;

    params.wheel_radius_m = DefaultParams::WHEEL_RADIUS_M;
    params.wheel_base_m = DefaultParams::WHEEL_BASE_M;
    params.encoder_ppr = DefaultParams::ENCODER_PPR;
    params.rad_per_pulse = DefaultParams::RAD_PER_PULSE;

    params.kp_right = DefaultParams::KP_RIGHT;
    params.ki_right = DefaultParams::KI_RIGHT;
    params.kd_right = DefaultParams::KD_RIGHT;
    params.ff_right = DefaultParams::FF_RIGHT;

    params.kp_left = DefaultParams::KP_LEFT;
    params.ki_left = DefaultParams::KI_LEFT;
    params.kd_left = DefaultParams::KD_LEFT;
    params.ff_left = DefaultParams::FF_LEFT;

    params.max_velocity = DefaultParams::MAX_VELOCITY;
    params.max_acceleration = DefaultParams::MAX_ACCELERATION;
    params.max_deceleration = DefaultParams::MAX_DECELERATION;
    params.min_pwm_deadband = DefaultParams::MIN_PWM_DEADBAND;
    params.max_pwm_slew_rate = DefaultParams::MAX_PWM_SLEW_RATE;

    params.gyro_bias_x = DefaultParams::GYRO_BIAS_X;
    params.gyro_bias_y = DefaultParams::GYRO_BIAS_Y;
    params.gyro_bias_z = DefaultParams::GYRO_BIAS_Z;

    params.invert_right_encoder = DefaultParams::INVERT_RIGHT_ENCODER;
    params.invert_left_encoder = DefaultParams::INVERT_LEFT_ENCODER;
    params.invert_right_motor = DefaultParams::INVERT_RIGHT_MOTOR;
    params.invert_left_motor = DefaultParams::INVERT_LEFT_MOTOR;
    
    params.crc16 = 0;
}

uint16_t EEPROMManager::calculateCRC16(const uint8_t* data, size_t length) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}
