/**
 * ============================================================================
 * LIFTCONTROLLER.H - Lift Mechanism State Machine & Electromagnet Control
 * ============================================================================
 * Controls the vertical lift mechanism for mine pickup and disposal.
 *
 * Features:
 *   - State machine: IDLE → RAISING → RAISED → LOWERING → LOWERED → IDLE
 *   - Limit switch integration (top/bottom, active LOW, debounced)
 *   - Stall timeout protection
 *   - 5 individually addressable electromagnets
 *   - Motor protection (auto-stop on limit switch)
 *
 * @file   LiftController.h
 * @author Assiut Robotics Team
 * @date   2026
 * ============================================================================
 */

#ifndef LIFTCONTROLLER_H
#define LIFTCONTROLLER_H

#include "Config.h"
#include <Arduino.h>

/**
 * @brief Lift mechanism operating states.
 */
enum class LiftState : uint8_t {
    IDLE,       ///< Lift at bottom, magnet ON/armed, waiting for detection or command
    RAISING,    ///< Lift motor driving upward with caught mine
    DROPPING,   ///< Reached top, motor stopped, magnet OFF (300ms dwell to drop mine)
    LOWERING,   ///< Lift motor driving downward back to bottom
    FAULT       ///< Stall timeout or limit switch error
};

/**
 * @brief Lift mechanism controller with state machine and electromagnets.
 *
 * Drives a DC motor via PWM/DIR for vertical lift movement.
 * Monitors top and bottom limit switches.
 * Monitors optical break-beam sensor on gripper (Pin 25) to auto-trigger mine pickup.
 * Controls electromagnet for mine retention and release.
 *
 * Automated Cycle:
 *   IDLE (at bottom, magnet ON)
 *     → [Break-beam detects mine OR manual trigger]
 *     → RAISING (motor drives UP)
 *     → [Hits Top Limit Switch]
 *     → DROPPING (motor stops, magnet turns OFF, waits 300 ms)
 *     → LOWERING (motor drives DOWN)
 *     → [Hits Bottom Limit Switch]
 *     → IDLE (motor stops, magnet turns ON)
 */
class LiftController {
public:
    /**
     * @brief Construct a new Lift Controller.
     */
    LiftController();

    /**
     * @brief Initialize all lift hardware (motor, switches, magnets, optical sensor).
     */
    void begin();

    /**
     * @brief Update the lift state machine.
     *
     * Must be called periodically (recommended: every 50 ms).
     * Reads limit switches, metal detector, and object detection status, updates automated cycle.
     * @param object_detected true if ultrasonic distance detects a mine caught beneath gripper
     * @param metal_detected  true if induction metal detector senses metal
     */
    void update(bool object_detected = false, bool metal_detected = false);

    /**
     * @brief Trigger an automated pickup and drop cycle (from joystick or command).
     */
    void triggerCycle();

    /**
     * @brief Command the lift to raise (calls triggerCycle).
     */
    void raise();

    /**
     * @brief Command the lift to lower.
     */
    void lower();

    /**
     * @brief Stop the lift motor immediately.
     */
    void stop();

    /**
     * @brief Direct manual joystick control of gripper velocity
     * @param g_vel Positive raises lift, negative lowers lift, ~0 stops
     */
    void handleVelocityCommand(float g_vel);

    /**
     * @brief Set an individual electromagnet state.
     * @param index Magnet index [0-4]. Values >= NUM_MAGNETS are ignored.
     * @param on    true to energize, false to de-energize
     */
    void setMagnet(uint8_t index, bool on);

    /**
     * @brief Set all electromagnets to the same state.
     * @param on true to energize all, false to de-energize all
     */
    void setAllMagnets(bool on);

    /**
     * @brief Set manual magnet override flag.
     * When true, magnets will stay on until manually turned off by operator.
     */
    void setManualMagnetOverride(bool override_on) { manual_magnet_override_ = override_on; }
    bool isManualMagnetOverride() const { return manual_magnet_override_; }

    /**
     * @brief Get the current lift state.
     * @return Current LiftState enum value
     */
    LiftState getState() const { return state_; }

    /**
     * @brief Get a human-readable string for the current state.
     * @return Pointer to a static string (e.g., "IDLE", "RAISING")
     */
    const char* getStateString() const;

    /**
     * @brief Check if the top limit switch is currently active.
     */
    bool isAtTop() const;

    /**
     * @brief Check if the bottom limit switch is currently active.
     */
    bool isAtBottom() const;

    /**
     * @brief Check if the gripper optical sensor currently detects an object.
     */
    bool isObjectDetected() const { return sensor_stable_; }

    /**
     * @brief Check if a lift cycle is currently in progress.
     */
    bool isCycling() const {
        return state_ == LiftState::RAISING || state_ == LiftState::DROPPING || state_ == LiftState::LOWERING;
    }

    /**
     * @brief Check if the electromagnet is currently active.
     */
    bool isMagnetActive() const { return (magnet_state_ != 0); }

    /**
     * @brief Check if the lift is in FAULT state.
     */
    bool hasFault() const { return state_ == LiftState::FAULT; }

    /**
     * @brief Clear the fault condition and return to IDLE.
     */
    void clearFault();

    /**
     * @brief Get the current magnet state as a bitmask.
     */
    uint8_t getMagnetState() const { return magnet_state_; }

    /**
     * @brief Raw limit switch states (instant, non-debounced).
     */
    bool isTopSwitchRaw() const { return isSwitchRawActive(Pins::LIMIT_SW_TOP); }
    bool isBottomSwitchRaw() const { return isSwitchRawActive(Pins::LIMIT_SW_BOTTOM); }

    /**
     * @brief Apply hardware or software PWM for motor speed.
     */
    void applyPWM();

private:
    LiftState state_;               ///< Current state machine state
    uint8_t magnet_state_;          ///< Bitmask of energized magnets
    unsigned long motion_start_ms_; ///< millis() when motor motion began
    unsigned long dwell_start_ms_;  ///< millis() when top drop dwell started
    unsigned long magnet_on_time_ms_; ///< millis() when magnet was energized in IDLE
    unsigned long debounce_top_ms_; ///< Last time top switch changed
    unsigned long debounce_bot_ms_; ///< Last time bottom switch changed
    unsigned long debounce_sensor_ms_; ///< Last time optical sensor changed
    bool top_switch_stable_;        ///< Debounced top switch state
    bool bot_switch_stable_;        ///< Debounced bottom switch state
    bool sensor_stable_;            ///< Debounced optical sensor state
    bool sensor_prev_state_;        ///< Edge detection for optical sensor
    bool manual_trigger_;           ///< Flag for joystick/serial trigger
    bool manual_magnet_override_;   ///< Operator manual magnet hold flag
    bool is_auto_cycling_;          ///< Automated pickup/drop sequence is actively running
    bool manual_active_;            ///< Manual joystick movement is actively commanding motor
    bool motor_running_;            ///< Lift motor active flag
    bool motor_dir_up_;             ///< Lift motor current direction (true = UP, false = DOWN)

    /**
     * @brief Drive the lift motor at configured speed.
     * @param up true = raise (DIR HIGH), false = lower (DIR LOW)
     */
    void driveMotor(bool up);

    /**
     * @brief Stop the lift motor (PWM = 0).
     */
    void stopMotor();

    /**
     * @brief Read and debounce a limit switch.
     * @param pin          Switch pin number
     * @param last_change  Reference to the debounce timestamp
     * @param stable_state Reference to the stable state variable
     * @return Debounced switch state (true = active/pressed)
     */
    bool readSwitch(uint8_t pin, unsigned long& last_change, bool& stable_state) const;

    /** @brief Raw pressed detection honoring LIMIT_SWITCH_ACTIVE_HIGH. */
    bool isSwitchRawActive(uint8_t pin) const;
};

#endif // LIFTCONTROLLER_H
