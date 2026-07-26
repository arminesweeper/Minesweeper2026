/**
 * ============================================================================
 * IMU.H - MPU6050 Inertial Measurement Unit Driver
 * ============================================================================
 */

#ifndef IMU_H
#define IMU_H

#include "Config.h"
#include <Arduino.h>

struct IMUData {
    float yaw;         ///< Yaw angle (degrees, integrated gyro Z)
    float pitch;       ///< Pitch angle (degrees, complementary filter)
    float roll;        ///< Roll angle (degrees, complementary filter)
    float gyro_x;      ///< Angular velocity X (deg/s)
    float gyro_y;      ///< Angular velocity Y (deg/s)
    float gyro_z;      ///< Angular velocity Z (deg/s)
    float accel_x;     ///< Linear acceleration X (g)
    float accel_y;     ///< Linear acceleration Y (g)
    float accel_z;     ///< Linear acceleration Z (g)
    float temperature; ///< Die temperature (Celsius)
};

class IMU {
public:
    IMU();

    /**
     * @brief Initialize MPU6050 and set config registers.
     * Does NOT block for calibration. Loads bias from arguments.
     */
    bool begin(float bias_x, float bias_y, float bias_z);

    /**
     * @brief Read sensors and update filtered orientation angles.
     * @param dt_sec Time since last call in seconds
     */
    void update(float dt_sec);

    /**
     * @brief Performs a blocking calibration (1000 samples).
     * @param out_bias_x Computed bias X
     * @param out_bias_y Computed bias Y
     * @param out_bias_z Computed bias Z
     */
    void calibrateGyro(float& out_bias_x, float& out_bias_y, float& out_bias_z);

    void setBias(float bias_x, float bias_y, float bias_z);

    bool isConnected() const;

    const IMUData& getData() const { return data_; }
    float getYaw() const { return data_.yaw; }
    float getPitch() const { return data_.pitch; }
    float getRoll() const { return data_.roll; }
    float getGyroZ() const { return data_.gyro_z; }
    float getTemperature() const { return data_.temperature; }

    void resetYaw();

    bool hasError() const { return error_flag_; }
    void clearError() { error_flag_ = false; }

private:
    IMUData data_;
    bool initialized_;
    bool error_flag_;

    float gyro_bias_x_;
    float gyro_bias_y_;
    float gyro_bias_z_;

    bool writeRegister(uint8_t reg, uint8_t value);
    uint8_t readRegister(uint8_t reg);
    bool readRegisters(uint8_t reg, uint8_t* buffer, uint8_t count);
};

#endif // IMU_H
