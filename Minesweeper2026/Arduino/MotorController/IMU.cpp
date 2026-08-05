/**
 * ============================================================================
 * IMU.CPP - MPU6050 Inertial Measurement Unit Implementation
 * ============================================================================
 * Supports:
 *   - Hardware Wire (D20/D21) when IMU_USE_SOFT_I2C == 0
 *   - Bit-bang I2C on Pins::IMU_SDA / IMU_SCL when IMU_USE_SOFT_I2C == 1
 *     (required on as-fabricated shield: D20/D21 are ENC1)
 *
 * Soft I2C is open-drain style (INPUT_PULLUP = released high).
 * No dynamic allocation. Not ISR-safe — call only from task context.
 *
 * @file   IMU.cpp
 * @author Assiut Robotics Team
 * @date   2026
 * ============================================================================
 */

#include "IMU.h"
#include <avr/wdt.h>
#include <math.h>
#include <string.h>

#if !IMU_USE_SOFT_I2C
#include <Wire.h>
#endif

#define MPU6050_REG_PWR_MGMT_1   0x6B
#define MPU6050_REG_GYRO_CONFIG  0x1B
#define MPU6050_REG_ACCEL_CONFIG 0x1C
#define MPU6050_REG_CONFIG       0x1A
#define MPU6050_REG_WHO_AM_I     0x75
#define MPU6050_REG_ACCEL_XOUT_H 0x3B

/* ============================================================================
 * SOFT I2C (bit-bang) — used when IMU_USE_SOFT_I2C == 1
 * ============================================================================ */

#if IMU_USE_SOFT_I2C

namespace SoftI2C {
    /* ~100 kHz-ish on 16 MHz AVR; keep short so IMU task stays bounded */
    static void delayHalf() {
        delayMicroseconds(5);
    }

    static void sdaHigh() {
        pinMode(Pins::IMU_SDA, INPUT_PULLUP);
    }

    static void sdaLow() {
        pinMode(Pins::IMU_SDA, OUTPUT);
        digitalWrite(Pins::IMU_SDA, LOW);
    }

    static void sclHigh() {
        pinMode(Pins::IMU_SCL, INPUT_PULLUP);
        /* Optional clock stretch wait (bounded) */
        uint16_t guard = 1000;
        while (digitalRead(Pins::IMU_SCL) == LOW && guard > 0) {
            --guard;
        }
    }

    static void sclLow() {
        pinMode(Pins::IMU_SCL, OUTPUT);
        digitalWrite(Pins::IMU_SCL, LOW);
    }

    static bool readSda() {
        return digitalRead(Pins::IMU_SDA) != LOW;
    }

    static void begin() {
        sdaHigh();
        sclHigh();
    }

    static void start() {
        sdaHigh();
        sclHigh();
        delayHalf();
        sdaLow();
        delayHalf();
        sclLow();
    }

    static void stop() {
        sdaLow();
        delayHalf();
        sclHigh();
        delayHalf();
        sdaHigh();
        delayHalf();
    }

    static bool writeByte(uint8_t data) {
        for (uint8_t i = 0; i < 8; ++i) {
            if (data & 0x80) {
                sdaHigh();
            } else {
                sdaLow();
            }
            delayHalf();
            sclHigh();
            delayHalf();
            sclLow();
            data <<= 1;
        }
        sdaHigh(); /* release for ACK */
        delayHalf();
        sclHigh();
        delayHalf();
        const bool ack = !readSda(); /* ACK = SDA low */
        sclLow();
        return ack;
    }

    static uint8_t readByte(bool send_ack) {
        uint8_t data = 0;
        sdaHigh();
        for (uint8_t i = 0; i < 8; ++i) {
            data <<= 1;
            delayHalf();
            sclHigh();
            delayHalf();
            if (readSda()) {
                data |= 0x01;
            }
            sclLow();
        }
        if (send_ack) {
            sdaLow();
        } else {
            sdaHigh();
        }
        delayHalf();
        sclHigh();
        delayHalf();
        sclLow();
        sdaHigh();
        return data;
    }

    static bool writeReg(uint8_t addr, uint8_t reg, uint8_t value) {
        start();
        if (!writeByte(static_cast<uint8_t>(addr << 1))) {
            stop();
            return false;
        }
        if (!writeByte(reg)) {
            stop();
            return false;
        }
        if (!writeByte(value)) {
            stop();
            return false;
        }
        stop();
        return true;
    }

    static bool readRegs(uint8_t addr, uint8_t reg, uint8_t* buffer, uint8_t count) {
        start();
        if (!writeByte(static_cast<uint8_t>(addr << 1))) {
            stop();
            return false;
        }
        if (!writeByte(reg)) {
            stop();
            return false;
        }
        start(); /* repeated start */
        if (!writeByte(static_cast<uint8_t>((addr << 1) | 0x01))) {
            stop();
            return false;
        }
        for (uint8_t i = 0; i < count; ++i) {
            buffer[i] = readByte(i + 1 < count); /* ACK all but last */
        }
        stop();
        return true;
    }
} /* namespace SoftI2C */

#endif /* IMU_USE_SOFT_I2C */

/* ============================================================================
 * IMU CLASS
 * ============================================================================ */

IMU::IMU() : initialized_(false), error_flag_(false),
             gyro_bias_x_(0.0f), gyro_bias_y_(0.0f), gyro_bias_z_(0.0f) {
    memset(&data_, 0, sizeof(IMUData));
}

bool IMU::begin(float bias_x, float bias_y, float bias_z) {
#if IMU_USE_SOFT_I2C
    SoftI2C::begin();
#else
    Wire.begin();
    Wire.setClock(400000);
#endif

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
#if IMU_USE_SOFT_I2C
    return SoftI2C::writeReg(IMUConfig::MPU6050_ADDR, reg, value);
#else
    Wire.beginTransmission(IMUConfig::MPU6050_ADDR);
    Wire.write(reg);
    Wire.write(value);
    return (Wire.endTransmission() == 0);
#endif
}

uint8_t IMU::readRegister(uint8_t reg) {
    uint8_t value = 0;
#if IMU_USE_SOFT_I2C
    if (!SoftI2C::readRegs(IMUConfig::MPU6050_ADDR, reg, &value, 1)) {
        return 0;
    }
    return value;
#else
    Wire.beginTransmission(IMUConfig::MPU6050_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return 0;

    Wire.requestFrom(IMUConfig::MPU6050_ADDR, (uint8_t)1);
    if (Wire.available()) return Wire.read();
    return 0;
#endif
}

bool IMU::readRegisters(uint8_t reg, uint8_t* buffer, uint8_t count) {
#if IMU_USE_SOFT_I2C
    return SoftI2C::readRegs(IMUConfig::MPU6050_ADDR, reg, buffer, count);
#else
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
#endif
}
