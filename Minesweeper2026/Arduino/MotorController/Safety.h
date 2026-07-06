/**
 * ============================================================================
 * SAFETY.H - Safety Systems & Fault Detection
 * ============================================================================
 * Provides multi-layer safety monitoring:
 *   - ATmega2560 hardware watchdog timer (2-second timeout)
 *   - Serial command timeout detection
 *   - Battery voltage monitoring (low/high thresholds)
 *   - Encoder fault detection
 *   - IMU fault detection
 *   - Lift fault detection
 *   - System state machine with automatic recovery
 *
 * @file   Safety.h
 * @author Assiut Robotics Team
 * @date   2026
 * ============================================================================
 */

#ifndef SAFETY_H
#define SAFETY_H

#include "Config.h"
#include <Arduino.h>

/**
 * @brief System operating state enumeration.
 *
 * Determines whether motors are allowed to run and what recovery
 * actions are available.
 */
enum class SystemState : uint8_t {
    BOOTING,        ///< System initializing, motors disabled
    ACTIVE,         ///< Normal operation, motors enabled
    ESTOP_TIMEOUT,  ///< Command timeout triggered, motors disabled
    ESTOP_MANUAL,   ///< Manual emergency stop, motors disabled
    FAULT_ENCODER,  ///< Encoder error detected, motors disabled
    FAULT_BATTERY,  ///< Battery voltage out of range, motors disabled
    FAULT_IMU,      ///< IMU communication failure, motors disabled
    FAULT_LIFT,     ///< Lift mechanism fault, lift motor disabled
    FAULT_UNKNOWN   ///< Unknown fault condition
};

/**
 * @brief Bit-packed fault flags (fits in one byte).
 *
 * Each flag indicates a specific fault condition.  Multiple faults
 * can be active simultaneously.
 */
struct FaultFlags {
    bool command_timeout : 1;  ///< Serial command timeout active
    bool encoder_right   : 1;  ///< Right encoder fault
    bool encoder_left    : 1;  ///< Left encoder fault
    bool battery_low     : 1;  ///< Battery voltage below LOW threshold
    bool battery_high    : 1;  ///< Battery voltage above HIGH threshold
    bool velocity_limit  : 1;  ///< Measured velocity exceeds sanity limit
    bool watchdog_reset  : 1;  ///< Previous boot was a watchdog reset
    bool imu_fault       : 1;  ///< IMU I2C communication error
};

/**
 * @brief Safety monitor with watchdog, timeout, and fault detection.
 *
 * Called every loop() iteration to check all safety conditions.
 * Transitions the system to the appropriate state when faults are
 * detected and allows automatic recovery when conditions clear.
 */
class SafetyMonitor {
public:
    /**
     * @brief Construct a new Safety Monitor.
     */
    SafetyMonitor();

    /**
     * @brief Initialize safety systems.
     *
     * Checks for previous watchdog reset (MCUSR.WDRF).
     * Enables hardware watchdog with 2-second timeout (if ENABLE_WATCHDOG).
     * Transitions to ACTIVE state.
     */
    void begin();

    /**
     * @brief Run all safety checks (call every loop() iteration).
     *
     * Checks:
     *   1. Command timeout (millis() - last_command_time > COMMAND_TIMEOUT_MS)
     *   2. Velocity sanity (measured velocity vs. MAX_VELOCITY_RAD_S)
     *   3. Battery voltage (via ADC on BATTERY_SENSE pin)
     *   4. Encoder fault flags
     *   5. IMU fault flag
     *
     * @param last_command_time millis() timestamp of last valid serial command
     * @param right_vel Current right wheel measured velocity (rad/s)
     * @param left_vel Current left wheel measured velocity (rad/s)
     * @return Current SystemState after all checks
     */
    SystemState update(unsigned long last_command_time,
                       double right_vel, double left_vel);

    /**
     * @brief Trigger a manual emergency stop.
     *
     * Transitions to ESTOP_MANUAL.  Requires clearEStop() to recover.
     */
    void triggerEStop();

    /**
     * @brief Clear emergency stop condition and return to ACTIVE.
     *
     * Works for both ESTOP_TIMEOUT and ESTOP_MANUAL states.
     * ESTOP_MANUAL requires a 100 ms debounce period.
     */
    void clearEStop();

    /**
     * @brief Get current system state.
     * @return Current SystemState
     */
    SystemState getState() const { return state_; }

    /**
     * @brief Check if system is in ACTIVE state (motors allowed).
     * @return true if state is ACTIVE
     */
    bool isActive() const { return state_ == SystemState::ACTIVE; }

    /**
     * @brief Get current fault flags.
     * @return Copy of the FaultFlags structure
     */
    FaultFlags getFaults() const { return faults_; }

    /**
     * @brief Check if any fault flag is set.
     * @return true if at least one fault is active
     */
    bool hasFault() const;

    /**
     * @brief Set or clear an encoder fault flag.
     * @param is_right true for right encoder, false for left
     * @param fault true to set fault, false to clear
     */
    void setEncoderFault(bool is_right, bool fault);

    /**
     * @brief Set or clear the IMU fault flag.
     * @param fault true to set fault, false to clear
     */
    void setIMUFault(bool fault);

    /**
     * @brief Set or clear the lift fault flag (via system state).
     * @param fault true to set FAULT_LIFT, false to clear
     */
    void setLiftFault(bool fault);

    /**
     * @brief Read battery voltage via ADC.
     * @return Battery voltage in millivolts
     */
    uint16_t readBatteryVoltage() const;

    /**
     * @brief Reset the hardware watchdog timer.
     *
     * Must be called frequently (at least once per WATCHDOG_TIMEOUT_MS).
     * Only active when ENABLE_WATCHDOG is defined.
     */
    void resetWatchdog();

    /**
     * @brief Get a human-readable string for the current state.
     * @return Pointer to a static string
     */
    const char* getStateString() const;

private:
    SystemState state_;            ///< Current system state
    FaultFlags faults_;            ///< Active fault flags
    unsigned long estop_trigger_time_; ///< millis() when E-Stop was triggered

    /**
     * @brief Transition to a new state (with optional diagnostic output).
     * @param new_state Target state
     */
    void setState(SystemState new_state);

    /**
     * @brief Check battery voltage and update fault flags.
     */
    void checkBatteryVoltage();
};

#endif // SAFETY_H