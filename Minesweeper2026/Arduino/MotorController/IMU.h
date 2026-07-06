/**
 * ============================================================================
 * IMU.H - MPU6050 Inertial Measurement Unit Driver
 * ============================================================================
 * Direct I2C register-level driver for the InvenSense MPU6050 6-axis IMU.
 * Provides yaw, pitch, and roll via complementary filter.
 *
 * Features:
 *   - No external library dependency (Wire only)
 *   - Gyroscope bias calibration on startup
 *   - Complementary filter for roll/pitch estimation
 *   - Gyro Z integration for yaw estimation
 *   - I2C health monitoring with error flag
 *   - Non-blocking operation (no delay() calls)
 *
 * @file   IMU.h
 * @author Assiut Robotics Team
 * @date   2026
 * ============================================================================
 */

#ifndef IMU_H
#define IMU_H

#include "Config.h"
#include <Arduino.h>

/**
 * @brief Processed IMU measurement data.
 *
 * All angular values are in degrees.
 * All angular velocities are in degrees per second.
 * Acceleration values are in g (gravitational units).
 */
struct IMUData {
    double yaw;         ///< Yaw angle (degrees, integrated gyro Z, drift-prone)
    double pitch;       ///< Pitch angle (degrees, complementary filter)
    double roll;        ///< Roll angle (degrees, complementary filter)
    double gyro_x;      ///< Angular velocity X (deg/s, bias-corrected)
    double gyro_y;      ///< Angular velocity Y (deg/s, bias-corrected)
    double gyro_z;      ///< Angular velocity Z (deg/s, bias-corrected)
    double accel_x;     ///< Linear acceleration X (g)
    double accel_y;     ///< Linear acceleration Y (g)
    double accel_z;     ///< Linear acceleration Z (g)
    double temperature; ///< Die temperature (degrees Celsius)
};

/**
 * @brief MPU6050 IMU driver with complementary filter.
 *
 * Uses direct I2C register access via the Arduino Wire library.
 * No external IMU library is required.
 *
 * Startup sequence:
 *   1. Verify device identity (WHO_AM_I register)
 *   2. Wake device from sleep
 *   3. Configure gyroscope range (±250 deg/s)
 *   4. Configure accelerometer range (±2 g)
 *   5. Set digital low-pass filter (44 Hz bandwidth)
 *   6. Calibrate gyroscope bias (average N samples at rest)
 *
 * Runtime:
 *   - Burst-read 14 bytes per update (accel XYZ + temp + gyro XYZ)
 *   - Subtract gyroscope bias from raw readings
 *   - Apply complementary filter for pitch and roll
 *   - Integrate gyro Z for yaw (drift-prone but adequate for short runs)
 */
class IMU {
public:
    /**
     * @brief Construct a new IMU driver instance.
     */
    IMU();

    /**
     * @brief Initialize the MPU6050 and calibrate the gyroscope.
     *
     * This function blocks for approximately 2.5 seconds during gyro
     * calibration (500 samples at ~5 ms each).  The watchdog timer is
     * reset during calibration to prevent a WDT reset.
     *
     * @return true if the MPU6050 responded correctly on I2C
     * @return false if the device was not found or WHO_AM_I mismatch
     */
    bool begin();

    /**
     * @brief Read sensors and update filtered orientation angles.
     *
     * Must be called periodically (recommended: 50 Hz / every 20 ms).
     * Non-blocking — performs a single I2C burst read.
     *
     * @param dt_sec Time since last call in seconds
     */
    void update(double dt_sec);

    /**
     * @brief Check if the MPU6050 is responding on I2C.
     * @return true if WHO_AM_I register returns expected value
     */
    bool isConnected() const;

    /**
     * @brief Get the most recent processed IMU data.
     * @return Const reference to the IMUData structure
     */
    const IMUData& getData() const { return data_; }

    /** @brief Get yaw angle in degrees (integrated gyro Z). */
    double getYaw() const { return data_.yaw; }

    /** @brief Get pitch angle in degrees (complementary filter). */
    double getPitch() const { return data_.pitch; }

    /** @brief Get roll angle in degrees (complementary filter). */
    double getRoll() const { return data_.roll; }

    /** @brief Get Z-axis angular velocity in deg/s (bias-corrected). */
    double getGyroZ() const { return data_.gyro_z; }

    /** @brief Get die temperature in degrees Celsius. */
    double getTemperature() const { return data_.temperature; }

    /**
     * @brief Reset the yaw integrator to zero.
     *
     * Call this when the robot's heading reference is re-established
     * (e.g., after realignment with a known direction).
     */
    void resetYaw();

    /**
     * @brief Check if an I2C communication error has occurred.
     * @return true if the last I2C transaction failed
     */
    bool hasError() const { return error_flag_; }

    /**
     * @brief Clear the error flag.
     *
     * The next successful I2C read will also clear the flag.
     */
    void clearError() { error_flag_ = false; }

private:
    IMUData data_;          ///< Most recent processed measurements
    bool initialized_;      ///< True after successful begin()
    bool error_flag_;       ///< True if I2C error detected

    /* Gyroscope bias (determined during calibration) */
    double gyro_bias_x_;    ///< X-axis gyro bias (deg/s)
    double gyro_bias_y_;    ///< Y-axis gyro bias (deg/s)
    double gyro_bias_z_;    ///< Z-axis gyro bias (deg/s)

    /**
     * @brief Write a single byte to an MPU6050 register.
     * @param reg Register address
     * @param value Byte value to write
     * @return true if write succeeded (no I2C error)
     */
    bool writeRegister(uint8_t reg, uint8_t value);

    /**
     * @brief Read a single byte from an MPU6050 register.
     * @param reg Register address
     * @return Register value, or 0 on error
     */
    uint8_t readRegister(uint8_t reg);

    /**
     * @brief Burst-read multiple bytes from consecutive registers.
     * @param reg Starting register address
     * @param buffer Destination buffer
     * @param count Number of bytes to read
     * @return true if read succeeded
     */
    bool readRegisters(uint8_t reg, uint8_t* buffer, uint8_t count);

    /**
     * @brief Calibrate the gyroscope by averaging samples at rest.
     *
     * The robot must be stationary during calibration.
     * Resets the watchdog timer periodically to prevent WDT reset.
     */
    void calibrateGyro();
};

#endif // IMU_H
