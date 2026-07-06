/**
 * ============================================================================
 * IMU.CPP - MPU6050 Inertial Measurement Unit Implementation
 * ============================================================================
 * Direct I2C register-level driver for the MPU6050.
 *
 * Register map reference: InvenSense MPU-6050 Register Map (RM-MPU-6000A-00)
 *
 * @file   IMU.cpp
 * @author Assiut Robotics Team
 * @date   2026
 * ============================================================================
 */

#include "IMU.h"
#include <Wire.h>
#include <avr/wdt.h>
#include <math.h>

/* ============================================================================
 * MPU6050 REGISTER ADDRESSES
 * ============================================================================ */

namespace MPU6050Reg {
    constexpr uint8_t WHO_AM_I      = 0x75;  ///< Device identity register
    constexpr uint8_t PWR_MGMT_1    = 0x6B;  ///< Power management 1
    constexpr uint8_t CONFIG        = 0x1A;  ///< Configuration (DLPF)
    constexpr uint8_t GYRO_CONFIG   = 0x1B;  ///< Gyroscope configuration
    constexpr uint8_t ACCEL_CONFIG  = 0x1C;  ///< Accelerometer configuration
    constexpr uint8_t ACCEL_XOUT_H  = 0x3B;  ///< First data register (burst read)
    constexpr uint8_t TEMP_OUT_H    = 0x41;  ///< Temperature data high byte
} // namespace MPU6050Reg

/* ============================================================================
 * CONSTRUCTOR
 * ============================================================================ */

IMU::IMU()
    : initialized_(false),
      error_flag_(false),
      gyro_bias_x_(0.0),
      gyro_bias_y_(0.0),
      gyro_bias_z_(0.0) {
    data_.yaw = 0.0;
    data_.pitch = 0.0;
    data_.roll = 0.0;
    data_.gyro_x = 0.0;
    data_.gyro_y = 0.0;
    data_.gyro_z = 0.0;
    data_.accel_x = 0.0;
    data_.accel_y = 0.0;
    data_.accel_z = 0.0;
    data_.temperature = 0.0;
}

/* ============================================================================
 * INITIALIZATION
 * ============================================================================ */

bool IMU::begin() {
    Wire.begin();
    Wire.setClock(400000);  /* 400 kHz I2C (Fast Mode) */

    /* Verify device identity */
    uint8_t who = readRegister(MPU6050Reg::WHO_AM_I);
    if (who != IMUConfig::WHO_AM_I_EXPECTED) {
        error_flag_ = true;
        return false;
    }

    /* Wake from sleep (clear SLEEP bit, select internal 8 MHz oscillator) */
    writeRegister(MPU6050Reg::PWR_MGMT_1, 0x00);
    delay(10);  /* Allow PLL to stabilize — only blocking delay in IMU init */

    /* Set gyroscope range: ±250 deg/s (FS_SEL = 0) */
    writeRegister(MPU6050Reg::GYRO_CONFIG, 0x00);

    /* Set accelerometer range: ±2 g (AFS_SEL = 0) */
    writeRegister(MPU6050Reg::ACCEL_CONFIG, 0x00);

    /* Set Digital Low-Pass Filter: ~44 Hz bandwidth (DLPF_CFG = 3) */
    writeRegister(MPU6050Reg::CONFIG, 0x03);

    /* Calibrate gyroscope bias (robot must be stationary) */
    calibrateGyro();

    initialized_ = true;
    error_flag_ = false;
    return true;
}

/* ============================================================================
 * GYROSCOPE CALIBRATION
 * ============================================================================ */

void IMU::calibrateGyro() {
    double sum_x = 0.0;
    double sum_y = 0.0;
    double sum_z = 0.0;
    uint8_t buf[14];

    for (uint16_t i = 0; i < IMUConfig::CALIBRATION_SAMPLES; ++i) {
        if (readRegisters(MPU6050Reg::ACCEL_XOUT_H, buf, 14)) {
            /* Gyro data starts at byte offset 8 (after accel[6] + temp[2]) */
            int16_t raw_gx = (static_cast<int16_t>(buf[8])  << 8) | buf[9];
            int16_t raw_gy = (static_cast<int16_t>(buf[10]) << 8) | buf[11];
            int16_t raw_gz = (static_cast<int16_t>(buf[12]) << 8) | buf[13];

            sum_x += static_cast<double>(raw_gx) / IMUConfig::GYRO_SCALE;
            sum_y += static_cast<double>(raw_gy) / IMUConfig::GYRO_SCALE;
            sum_z += static_cast<double>(raw_gz) / IMUConfig::GYRO_SCALE;
        }

        /* Reset watchdog during calibration to prevent WDT reset */
#if ENABLE_WATCHDOG
        wdt_reset();
#endif
        delay(5);  /* ~5 ms between samples → ~2.5 s total calibration */
    }

    double n = static_cast<double>(IMUConfig::CALIBRATION_SAMPLES);
    gyro_bias_x_ = sum_x / n;
    gyro_bias_y_ = sum_y / n;
    gyro_bias_z_ = sum_z / n;
}

/* ============================================================================
 * UPDATE (CALLED PERIODICALLY)
 * ============================================================================ */

void IMU::update(double dt_sec) {
    if (!initialized_) {
        return;
    }

    if (dt_sec <= 0.0) {
        return;
    }

    /* Burst-read 14 bytes: accel XYZ (6) + temp (2) + gyro XYZ (6) */
    uint8_t buf[14];
    if (!readRegisters(MPU6050Reg::ACCEL_XOUT_H, buf, 14)) {
        error_flag_ = true;
        return;
    }
    error_flag_ = false;

    /* Parse raw accelerometer data (big-endian, 16-bit signed) */
    int16_t raw_ax = (static_cast<int16_t>(buf[0])  << 8) | buf[1];
    int16_t raw_ay = (static_cast<int16_t>(buf[2])  << 8) | buf[3];
    int16_t raw_az = (static_cast<int16_t>(buf[4])  << 8) | buf[5];

    /* Parse raw temperature */
    int16_t raw_temp = (static_cast<int16_t>(buf[6]) << 8) | buf[7];

    /* Parse raw gyroscope data */
    int16_t raw_gx = (static_cast<int16_t>(buf[8])  << 8) | buf[9];
    int16_t raw_gy = (static_cast<int16_t>(buf[10]) << 8) | buf[11];
    int16_t raw_gz = (static_cast<int16_t>(buf[12]) << 8) | buf[13];

    /* Convert to physical units */
    data_.accel_x = static_cast<double>(raw_ax) / IMUConfig::ACCEL_SCALE;
    data_.accel_y = static_cast<double>(raw_ay) / IMUConfig::ACCEL_SCALE;
    data_.accel_z = static_cast<double>(raw_az) / IMUConfig::ACCEL_SCALE;

    data_.gyro_x = (static_cast<double>(raw_gx) / IMUConfig::GYRO_SCALE) - gyro_bias_x_;
    data_.gyro_y = (static_cast<double>(raw_gy) / IMUConfig::GYRO_SCALE) - gyro_bias_y_;
    data_.gyro_z = (static_cast<double>(raw_gz) / IMUConfig::GYRO_SCALE) - gyro_bias_z_;

    /* Temperature: T_degC = raw / 340.0 + 36.53 (per datasheet) */
    data_.temperature = static_cast<double>(raw_temp) / 340.0 + 36.53;

    /* Complementary filter for pitch and roll */
    double accel_pitch = atan2(data_.accel_y,
                               sqrt(data_.accel_x * data_.accel_x +
                                    data_.accel_z * data_.accel_z)) *
                          (180.0 / 3.14159265359);

    double accel_roll = atan2(-data_.accel_x,
                               data_.accel_z) *
                         (180.0 / 3.14159265359);

    double alpha = IMUConfig::COMPLEMENTARY_ALPHA;
    data_.pitch = alpha * (data_.pitch + data_.gyro_y * dt_sec) +
                  (1.0 - alpha) * accel_pitch;
    data_.roll  = alpha * (data_.roll  + data_.gyro_x * dt_sec) +
                  (1.0 - alpha) * accel_roll;

    /* Integrate gyro Z for yaw (drift-prone, but usable for short durations) */
    data_.yaw += data_.gyro_z * dt_sec;

    /* Normalize yaw to [-180, 180] */
    while (data_.yaw > 180.0) {
        data_.yaw -= 360.0;
    }
    while (data_.yaw < -180.0) {
        data_.yaw += 360.0;
    }
}

/* ============================================================================
 * STATUS QUERIES
 * ============================================================================ */

bool IMU::isConnected() const {
    /* Non-const cast required for Wire operations */
    IMU* self = const_cast<IMU*>(this);
    uint8_t who = self->readRegister(MPU6050Reg::WHO_AM_I);
    return (who == IMUConfig::WHO_AM_I_EXPECTED);
}

void IMU::resetYaw() {
    data_.yaw = 0.0;
}

/* ============================================================================
 * I2C REGISTER ACCESS
 * ============================================================================ */

bool IMU::writeRegister(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(IMUConfig::MPU6050_ADDR);
    Wire.write(reg);
    Wire.write(value);
    uint8_t result = Wire.endTransmission();
    if (result != 0) {
        error_flag_ = true;
        return false;
    }
    return true;
}

uint8_t IMU::readRegister(uint8_t reg) {
    Wire.beginTransmission(IMUConfig::MPU6050_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) {
        error_flag_ = true;
        return 0;
    }

    Wire.requestFrom(IMUConfig::MPU6050_ADDR, static_cast<uint8_t>(1));
    if (Wire.available()) {
        return Wire.read();
    }

    error_flag_ = true;
    return 0;
}

bool IMU::readRegisters(uint8_t reg, uint8_t* buffer, uint8_t count) {
    Wire.beginTransmission(IMUConfig::MPU6050_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) {
        error_flag_ = true;
        return false;
    }

    Wire.requestFrom(IMUConfig::MPU6050_ADDR, count);
    for (uint8_t i = 0; i < count; ++i) {
        if (!Wire.available()) {
            error_flag_ = true;
            return false;
        }
        buffer[i] = Wire.read();
    }

    return true;
}
