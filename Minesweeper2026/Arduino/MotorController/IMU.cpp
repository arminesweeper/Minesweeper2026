/**
 * ============================================================================
 * IMU.CPP - MPU6050 Inertial Measurement Unit Implementation
 * ============================================================================
 */

#include "IMU.h"
#include <Wire.h>
#include <avr/wdt.h>
#include <math.h>

#define MPU6050_REG_PWR_MGMT_1   0x6B
#define MPU6050_REG_GYRO_CONFIG  0x1B
#define MPU6050_REG_ACCEL_CONFIG 0x1C
#define MPU6050_REG_CONFIG       0x1A
#define MPU6050_REG_WHO_AM_I     0x75
#define MPU6050_REG_ACCEL_XOUT_H 0x3B

IMU::IMU() : initialized_(false), error_flag_(false),
             gyro_bias_x_(0.0f), gyro_bias_y_(0.0f), gyro_bias_z_(0.0f) {
    memset(&data_, 0, sizeof(IMUData));
}

bool IMU::begin(float bias_x, float bias_y, float bias_z) {
    Wire.begin();
    Wire.setClock(400000); // 400kHz Fast I2C

    if (!isConnected()) {
        error_flag_ = true;
        return false;
    }

    // Wake up
    if (!writeRegister(MPU6050_REG_PWR_MGMT_1, 0x00)) return false;

    // Gyro Config: 0x00 = +/- 250 deg/s
    if (!writeRegister(MPU6050_REG_GYRO_CONFIG, 0x00)) return false;

    // Accel Config: 0x00 = +/- 2g
    if (!writeRegister(MPU6050_REG_ACCEL_CONFIG, 0x00)) return false;

    // DLPF Config: 0x03 = 44Hz bandwidth, ~4.9ms delay
    if (!writeRegister(MPU6050_REG_CONFIG, 0x03)) return false;

    setBias(bias_x, bias_y, bias_z);

    initialized_ = true;
    error_flag_ = false;
    return true;
}

void IMU::setBias(float bias_x, float bias_y, float bias_z) {
    gyro_bias_x_ = bias_x;
    gyro_bias_y_ = bias_y;
    gyro_bias_z_ = bias_z;
}

void IMU::calibrateGyro(float& out_bias_x, float& out_bias_y, float& out_bias_z) {
    float sum_x = 0.0f;
    float sum_y = 0.0f;
    float sum_z = 0.0f;
    const uint16_t samples = 1000;

    for (uint16_t i = 0; i < samples; i++) {
        uint8_t buffer[14];
        if (readRegisters(MPU6050_REG_ACCEL_XOUT_H, buffer, 14)) {
            int16_t gx = (buffer[8] << 8) | buffer[9];
            int16_t gy = (buffer[10] << 8) | buffer[11];
            int16_t gz = (buffer[12] << 8) | buffer[13];

            sum_x += static_cast<float>(gx) / IMUConfig::GYRO_SCALE;
            sum_y += static_cast<float>(gy) / IMUConfig::GYRO_SCALE;
            sum_z += static_cast<float>(gz) / IMUConfig::GYRO_SCALE;
        }
        delay(3);
        wdt_reset(); // Pet watchdog during blocking calibration
    }

    out_bias_x = sum_x / samples;
    out_bias_y = sum_y / samples;
    out_bias_z = sum_z / samples;

    setBias(out_bias_x, out_bias_y, out_bias_z);
}

void IMU::update(float dt_sec) {
    if (!initialized_ || error_flag_) return;

    uint8_t buffer[14];
    if (!readRegisters(MPU6050_REG_ACCEL_XOUT_H, buffer, 14)) {
        error_flag_ = true;
        return;
    }

    int16_t ax = (buffer[0] << 8) | buffer[1];
    int16_t ay = (buffer[2] << 8) | buffer[3];
    int16_t az = (buffer[4] << 8) | buffer[5];
    int16_t temp = (buffer[6] << 8) | buffer[7];
    int16_t gx = (buffer[8] << 8) | buffer[9];
    int16_t gy = (buffer[10] << 8) | buffer[11];
    int16_t gz = (buffer[12] << 8) | buffer[13];

    // Raw conversions
    data_.accel_x = static_cast<float>(ax) / IMUConfig::ACCEL_SCALE;
    data_.accel_y = static_cast<float>(ay) / IMUConfig::ACCEL_SCALE;
    data_.accel_z = static_cast<float>(az) / IMUConfig::ACCEL_SCALE;
    data_.temperature = (static_cast<float>(temp) / 340.0f) + 36.53f;

    data_.gyro_x = (static_cast<float>(gx) / IMUConfig::GYRO_SCALE) - gyro_bias_x_;
    data_.gyro_y = (static_cast<float>(gy) / IMUConfig::GYRO_SCALE) - gyro_bias_y_;
    data_.gyro_z = (static_cast<float>(gz) / IMUConfig::GYRO_SCALE) - gyro_bias_z_;

    // Calculate Accel Pitch/Roll
    float accel_pitch = atan2(data_.accel_y, sqrt(data_.accel_x * data_.accel_x + data_.accel_z * data_.accel_z)) * 180.0f / PI;
    float accel_roll = atan2(-data_.accel_x, data_.accel_z) * 180.0f / PI;

    // Complementary Filter
    if (dt_sec > 0.0f) {
        data_.pitch = IMUConfig::COMPLEMENTARY_ALPHA * (data_.pitch + data_.gyro_x * dt_sec) + 
                      (1.0f - IMUConfig::COMPLEMENTARY_ALPHA) * accel_pitch;
                      
        data_.roll = IMUConfig::COMPLEMENTARY_ALPHA * (data_.roll + data_.gyro_y * dt_sec) + 
                     (1.0f - IMUConfig::COMPLEMENTARY_ALPHA) * accel_roll;

        data_.yaw += data_.gyro_z * dt_sec;
    }
}

void IMU::resetYaw() {
    data_.yaw = 0.0f;
}

bool IMU::isConnected() const {
    IMU* self = const_cast<IMU*>(this);
    uint8_t who = self->readRegister(MPU6050_REG_WHO_AM_I);
    return (who == IMUConfig::WHO_AM_I_EXPECTED);
}

bool IMU::writeRegister(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(IMUConfig::MPU6050_ADDR);
    Wire.write(reg);
    Wire.write(value);
    return (Wire.endTransmission() == 0);
}

uint8_t IMU::readRegister(uint8_t reg) {
    Wire.beginTransmission(IMUConfig::MPU6050_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return 0;
    
    Wire.requestFrom(IMUConfig::MPU6050_ADDR, (uint8_t)1);
    if (Wire.available()) return Wire.read();
    return 0;
}

bool IMU::readRegisters(uint8_t reg, uint8_t* buffer, uint8_t count) {
    Wire.beginTransmission(IMUConfig::MPU6050_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    
    Wire.requestFrom(IMUConfig::MPU6050_ADDR, count);
    if (Wire.available() == count) {
        for (uint8_t i = 0; i < count; i++) {
            buffer[i] = Wire.read();
        }
        return true;
    }
    return false;
}
