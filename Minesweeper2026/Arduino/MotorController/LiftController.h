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
    IDLE,       ///< Lift at rest, ready for commands
    RAISING,    ///< Lift motor driving upward
    RAISED,     ///< Lift at top position (top limit switch active)
    LOWERING,   ///< Lift motor driving downward
    LOWERED,    ///< Lift at bottom position (bottom limit switch active)
    FAULT       ///< Stall timeout or limit switch error
};

/**
 * @brief Lift mechanism controller with state machine and electromagnets.
 *
 * Drives a DC motor via PWM/DIR for vertical lift movement.
 * Monitors top and bottom limit switches (normally open, active LOW,
 * connected to pins with internal pull-ups).
 * Controls 5 electromagnet channels for mine pickup.
 *
 * Includes stall timeout: if the motor runs longer than STALL_TIMEOUT_MS
 * without reaching a limit switch, the controller enters FAULT state and
 * stops the motor.
 */
class LiftController {
public:
    /**
     * @brief Construct a new Lift Controller.
     */
    LiftController();

    /**
     * @brief Initialize all lift hardware (motor, switches, magnets).
     *
     * Sets all magnet pins to OUTPUT (LOW).
     * Sets motor pins to OUTPUT (PWM=0, DIR=HIGH).
     * Enables internal pull-ups on limit switch pins.
     */
    void begin();

    /**
     * @brief Update the lift state machine.
     *
     * Must be called periodically (recommended: every 50 ms).
     * Reads limit switches, checks stall timeout, updates motor output.
     */
    void update();

    /**
     * @brief Command the lift to raise.
     *
     * Ignored if already RAISING or RAISED.
     * Transition: IDLE/LOWERED → RAISING.
     */
    void raise();

    /**
     * @brief Command the lift to lower.
     *
     * Ignored if already LOWERING or LOWERED.
     * Transition: RAISED → LOWERING.
     */
    void lower();

    /**
     * @brief Stop the lift motor immediately.
     *
     * Transition: any → IDLE (unless in FAULT).
     */
    void stop();

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
     * @return true if the switch pin reads LOW (active)
     */
    bool isAtTop() const;

    /**
     * @brief Check if the bottom limit switch is currently active.
     * @return true if the switch pin reads LOW (active)
     */
    bool isAtBottom() const;

    /**
     * @brief Check if the lift is in FAULT state.
     * @return true if a fault is active
     */
    bool hasFault() const { return state_ == LiftState::FAULT; }

    /**
     * @brief Clear the fault condition and return to IDLE.
     *
     * The motor is stopped before transitioning to IDLE.
     */
    void clearFault();

    /**
     * @brief Get the current magnet state as a bitmask.
     * @return Bits 0-4 correspond to magnets 1-5 (1 = energized)
     */
    uint8_t getMagnetState() const { return magnet_state_; }

private:
    LiftState state_;               ///< Current state machine state
    uint8_t magnet_state_;          ///< Bitmask of energized magnets
    unsigned long motion_start_ms_; ///< millis() when motor motion began
    unsigned long debounce_top_ms_; ///< Last time top switch changed
    unsigned long debounce_bot_ms_; ///< Last time bottom switch changed
    bool top_switch_stable_;        ///< Debounced top switch state
    bool bot_switch_stable_;        ///< Debounced bottom switch state

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
};

#endif // LIFTCONTROLLER_H
