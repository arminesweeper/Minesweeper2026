/**
 * ============================================================================
 * SENSORS.H - Peripheral Sensor Manager
 * ============================================================================
 * Manages all peripheral sensors and indicators:
 *   - 5 analog proximity sensors (with multi-sample averaging)
 *   - Metal detector (digital, with software debounce)
 *   - Buzzer (pattern-based tone output)
 *   - Warning LED (blink patterns)
 *
 * All operations are non-blocking (millis()-based scheduling).
 *
 * @file   Sensors.h
 * @author Assiut Robotics Team
 * @date   2026
 * ============================================================================
 */

#ifndef SENSORS_H
#define SENSORS_H

#include "Config.h"
#include <Arduino.h>

/**
 * @brief Buzzer sound pattern enumeration.
 */
enum class BuzzerPattern : uint8_t {
    SILENT,       ///< No sound output
    BEEP,         ///< Single short beep (50 ms on, then silent)
    ALERT,        ///< Slow beeping (200 ms on, 200 ms off)
    ALARM,        ///< Fast beeping (100 ms on, 100 ms off)
    MINE_DETECT   ///< Continuous tone at BUZZER_FREQ_HZ
};

/**
 * @brief Warning LED blink pattern enumeration.
 */
enum class LEDPattern : uint8_t {
    OFF,          ///< LED always off
    ON,           ///< LED always on
    SLOW_BLINK,   ///< 500 ms on, 500 ms off (1 Hz)
    FAST_BLINK,   ///< 100 ms on, 100 ms off (5 Hz)
    HEARTBEAT     ///< Double-pulse: ON 100ms, OFF 100ms, ON 100ms, OFF 600ms
};

/**
 * @brief Peripheral sensor and indicator manager.
 *
 * Handles all non-motor, non-IMU sensors connected to the Arduino:
 * - Proximity sensors: read via analogRead with configurable sample averaging.
 * - Metal detector: digital input with debounced edge detection.
 * - Buzzer: non-blocking pattern generator using tone()/noTone().
 * - Warning LED: non-blocking blink pattern generator.
 */
class Sensors {
public:
    /**
     * @brief Construct a new Sensors manager.
     */
    Sensors();

    /**
     * @brief Initialize all sensor and indicator pins.
     *
     * Configures proximity pins as analog input.
     * Configures metal detector with internal pull-up.
     * Configures buzzer and LED as outputs (initially off).
     */
    void begin();

    /**
     * @brief Update all sensor readings and indicator patterns.
     *
     * Should be called periodically (recommended: every 50 ms).
     * Reads proximity sensors, checks metal detector, updates
     * buzzer and LED patterns.
     */
    void update();

    /**
     * @brief Get the last-read value of a proximity sensor.
     * @param index Sensor index [0-4]
     * @return ADC value [0-1023], or 0 if index out of range
     */
    uint16_t getProximity(uint8_t index) const;

    /**
     * @brief Copy all 5 proximity values into a caller-provided array.
     * @param values Pointer to a uint16_t array of at least 5 elements
     */
    void getAllProximity(uint16_t* values) const;

    /**
     * @brief Check if a proximity sensor is above the detection threshold.
     * @param index Sensor index [0-4]
     * @return true if the sensor value exceeds PROXIMITY_THRESHOLD
     */
    bool isProximityTriggered(uint8_t index) const;

    /**
     * @brief Check if the metal detector is currently triggered.
     * @return true if metal is detected (debounced)
     */
    bool isMetalDetected() const { return metal_detected_; }

    /**
     * @brief Set the buzzer sound pattern.
     * @param pattern Desired BuzzerPattern
     */
    void setBuzzerPattern(BuzzerPattern pattern);

    /**
     * @brief Set the warning LED blink pattern.
     * @param pattern Desired LEDPattern
     */
    void setLEDPattern(LEDPattern pattern);

    /**
     * @brief Trigger a single short beep (non-blocking).
     *
     * Sets pattern to BEEP.  The pattern returns to SILENT
     * automatically after the beep duration.
     */
    void beep();

    /**
     * @brief Get the current buzzer pattern.
     * @return Current BuzzerPattern
     */
    BuzzerPattern getBuzzerPattern() const { return buzzer_pattern_; }

    /**
     * @brief Get the current LED pattern.
     * @return Current LEDPattern
     */
    LEDPattern getLEDPattern() const { return led_pattern_; }

private:
    /* Proximity sensor data */
    uint16_t proximity_values_[SensorConfig::NUM_PROXIMITY];

    /* Metal detector state */
    bool metal_detected_;           ///< Debounced metal detection state
    bool metal_raw_last_;           ///< Last raw reading
    unsigned long metal_change_ms_; ///< Timestamp of last raw state change

    /* Buzzer state */
    BuzzerPattern buzzer_pattern_;
    unsigned long buzzer_toggle_ms_; ///< Last buzzer on/off toggle
    bool buzzer_on_;                 ///< Current buzzer output state

    /* LED state */
    LEDPattern led_pattern_;
    unsigned long led_toggle_ms_;    ///< Last LED toggle
    bool led_on_;                    ///< Current LED output state
    uint8_t heartbeat_phase_;        ///< Heartbeat pattern phase counter

    /**
     * @brief Read a single proximity sensor with multi-sample averaging.
     * @param pin Analog pin number
     * @return Averaged ADC value [0-1023]
     */
    uint16_t readProximityAveraged(uint8_t pin) const;

    /**
     * @brief Read and debounce the metal detector input.
     */
    void updateMetalDetector();

    /**
     * @brief Update the buzzer output based on current pattern.
     */
    void updateBuzzer();

    /**
     * @brief Update the LED output based on current pattern.
     */
    void updateLED();
};

#endif // SENSORS_H
