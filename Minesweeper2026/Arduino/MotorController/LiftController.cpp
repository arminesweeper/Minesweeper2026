/**
 * ============================================================================
 * LIFTCONTROLLER.CPP - Automated Lift Mechanism Implementation
 * ============================================================================
 * @file   LiftController.cpp
 * @author Assiut Robotics Team
 * @date   2026
 * ============================================================================
 */

#include "LiftController.h"

/* ============================================================================
 * CONSTRUCTOR
 * ============================================================================ */

LiftController::LiftController()
    : state_(LiftState::IDLE),
      magnet_state_(0),
      motion_start_ms_(0),
      dwell_start_ms_(0),
      magnet_on_time_ms_(0),
      debounce_top_ms_(0),
      debounce_bot_ms_(0),
      debounce_sensor_ms_(0),
      top_switch_stable_(false),
      bot_switch_stable_(false),
      sensor_stable_(false),
      sensor_prev_state_(false),
      manual_trigger_(false),
      manual_magnet_override_(false) {}

/* ============================================================================
 * INITIALIZATION
 * ============================================================================ */

void LiftController::begin() {
    /* Configure lift motor pins */
    pinMode(Pins::LIFT_PWM, OUTPUT);
    pinMode(Pins::LIFT_DIR, OUTPUT);
    analogWrite(Pins::LIFT_PWM, 0);
    digitalWrite(Pins::LIFT_DIR, HIGH);

    /* Limit switches: polarity depends on LIMIT_SWITCH_ACTIVE_HIGH */
#if LIMIT_SWITCH_ACTIVE_HIGH
    pinMode(Pins::LIMIT_SW_TOP, INPUT);
    pinMode(Pins::LIMIT_SW_BOTTOM, INPUT);
#else
    pinMode(Pins::LIMIT_SW_TOP, INPUT_PULLUP);
    pinMode(Pins::LIMIT_SW_BOTTOM, INPUT_PULLUP);
#endif

    /* Electromagnet / shared relay pin(s) */
#if MAGNET_SHARED_RELAY
    pinMode(Pins::MAGNET_RELAY, OUTPUT);
    digitalWrite(Pins::MAGNET_RELAY, HIGH);
#else
    for (uint8_t i = 0; i < LiftConfig::NUM_MAGNETS; ++i) {
        pinMode(Pins::MAGNET_PINS[i], OUTPUT);
        digitalWrite(Pins::MAGNET_PINS[i], HIGH);
    }
#endif

    /* Magnet starts OFF in IDLE to prevent heating */
    setAllMagnets(false);
    state_ = LiftState::IDLE;
    manual_trigger_ = false;
    manual_magnet_override_ = false;
    magnet_on_time_ms_ = 0;

    /* Initialize debounce states */
    unsigned long now = millis();
    debounce_top_ms_ = now;
    debounce_bot_ms_ = now;
    debounce_sensor_ms_ = now;
    top_switch_stable_ = isSwitchRawActive(Pins::LIMIT_SW_TOP);
    bot_switch_stable_ = isSwitchRawActive(Pins::LIMIT_SW_BOTTOM);
    sensor_stable_ = false;
    sensor_prev_state_ = false;
}

/* ============================================================================
 * AUTOMATED STATE MACHINE UPDATE
 * ============================================================================ */

void LiftController::update(bool object_detected, bool metal_detected) {
    /* Read debounced switch states */
    bool at_top = readSwitch(Pins::LIMIT_SW_TOP, debounce_top_ms_, top_switch_stable_);
    bool at_bottom = readSwitch(Pins::LIMIT_SW_BOTTOM, debounce_bot_ms_, bot_switch_stable_);

    sensor_stable_ = object_detected;

    switch (state_) {
    case LiftState::IDLE: {
        /* If metal is detected, energize magnet to catch mine and start timeout */
        if (metal_detected && !isMagnetActive()) {
            setAllMagnets(true);
            magnet_on_time_ms_ = millis();
        }

        /* Auto-shutoff magnet if timeout passes in IDLE without picking up a mine (prevents heating).
         * Does NOT shut off if operator manually energized the magnets (manual_magnet_override_). */
        if (isMagnetActive() && !manual_trigger_ && !manual_magnet_override_ && (millis() - magnet_on_time_ms_ > LiftConfig::MAGNET_ARM_TIMEOUT_MS)) {
            setAllMagnets(false);
        }

        /* Trigger cycle if an object/mine newly enters ultrasonic detection range (edge trigger),
         * OR if triggered manually via command. */
        bool new_object = (object_detected && !sensor_prev_state_);
        sensor_prev_state_ = object_detected;

        if (new_object || manual_trigger_ || (object_detected && !at_top && (millis() - motion_start_ms_ > 2000))) {
            manual_trigger_ = false;
            if (!at_top) {
                setAllMagnets(true); /* Ensure magnet is ON while lifting */
                driveMotor(true);    /* Move UP */
                motion_start_ms_ = millis();
                state_ = LiftState::RAISING;
            }
        }
        break;
    }

    case LiftState::RAISING:
        sensor_prev_state_ = object_detected;
        if (at_top) {
            /* Reached top limit switch — stop motor, release magnet to drop mine (unless manual override), start dwell */
            stopMotor();
            if (!manual_magnet_override_) {
                setAllMagnets(false);
            }
            dwell_start_ms_ = millis();
            state_ = LiftState::DROPPING;
        } else if (millis() - motion_start_ms_ > LiftConfig::STALL_TIMEOUT_MS) {
            stopMotor();
            if (!manual_magnet_override_) {
                setAllMagnets(false);
            }
            state_ = LiftState::FAULT;
        }
        break;

    case LiftState::DROPPING:
        sensor_prev_state_ = object_detected;
        /* Wait for dwell with magnet OFF so mine falls clear into container */
        if (millis() - dwell_start_ms_ >= LiftConfig::DROP_DWELL_MS) {
            if (!at_bottom) {
                setAllMagnets(true); /* Magnet relay powers limit switches: must energize for descent */
                driveMotor(false);    /* Move DOWN */
                motion_start_ms_ = millis();
                state_ = LiftState::LOWERING;
            } else {
                if (!manual_magnet_override_) {
                    setAllMagnets(false);
                }
                state_ = LiftState::IDLE;
            }
        }
        break;

    case LiftState::LOWERING:
        sensor_prev_state_ = object_detected;
        if (at_bottom) {
            /* Reached bottom limit switch — stop motor */
            stopMotor();
            if (!manual_magnet_override_) {
                setAllMagnets(false);
            }
            state_ = LiftState::IDLE;
        } else if (millis() - motion_start_ms_ > LiftConfig::STALL_TIMEOUT_MS) {
            stopMotor();
            if (!manual_magnet_override_) {
                setAllMagnets(false);
            }
            state_ = LiftState::FAULT;
        }
        break;

    case LiftState::FAULT:
        stopMotor();
        if (!manual_magnet_override_) {
            setAllMagnets(false);
        }
        break;
    }
}

/* ============================================================================
 * COMMANDS
 * ============================================================================ */

void LiftController::triggerCycle() {
    if (state_ == LiftState::FAULT) {
        clearFault();
    }
    manual_trigger_ = true;
}

void LiftController::raise() {
    triggerCycle();
}

void LiftController::lower() {
    if (state_ == LiftState::DROPPING) {
        dwell_start_ms_ = 0; /* Force immediate descent */
    } else if (state_ == LiftState::FAULT) {
        clearFault();
    }
    if (!isAtBottom()) {
        setAllMagnets(true); /* Magnet relay powers limit switches: must energize for descent */
        driveMotor(false);
        motion_start_ms_ = millis();
        state_ = LiftState::LOWERING;
    }
}

void LiftController::stop() {
    stopMotor();
    state_ = LiftState::IDLE;
}

void LiftController::handleVelocityCommand(float g_vel) {
    if (g_vel > 0.1f) {
        // Raise upward
        if (state_ == LiftState::FAULT) clearFault();
        setAllMagnets(true);
        if (!isAtTop()) {
            driveMotor(true);
            motion_start_ms_ = millis();
            state_ = LiftState::RAISING;
        } else {
            stopMotor();
        }
    } else if (g_vel < -0.1f) {
        // Lower downward
        if (state_ == LiftState::FAULT) clearFault();
        setAllMagnets(true);
        if (!isAtBottom()) {
            driveMotor(false);
            motion_start_ms_ = millis();
            state_ = LiftState::LOWERING;
        } else {
            stopMotor();
        }
    } else {
        // Neutral joystick: stop if not running an automated cycle
        if (state_ == LiftState::RAISING || state_ == LiftState::LOWERING) {
            stopMotor();
            state_ = LiftState::IDLE;
        }
    }
}

void LiftController::clearFault() {
    stopMotor();
    state_ = LiftState::IDLE;
    manual_trigger_ = false;
}

/* ============================================================================
 * ELECTROMAGNET CONTROL
 * ============================================================================ */

void LiftController::setMagnet(uint8_t index, bool on) {
    if (index >= LiftConfig::NUM_MAGNETS) {
        return;
    }

    if (on) {
        magnet_state_ |= (1U << index);
        magnet_on_time_ms_ = millis();
    } else {
        magnet_state_ &= ~(1U << index);
    }

#if MAGNET_SHARED_RELAY
    /* Any bit set → energize shared relay */
    digitalWrite(Pins::MAGNET_RELAY, (magnet_state_ != 0) ? HIGH : LOW);
#else
    digitalWrite(Pins::MAGNET_PINS[index], on ? HIGH : LOW);
#endif
}

void LiftController::setAllMagnets(bool on) {
#if MAGNET_SHARED_RELAY
    magnet_state_ = on ? 0x1F : 0x00;
    digitalWrite(Pins::MAGNET_RELAY, on ? HIGH : LOW);
#else
    for (uint8_t i = 0; i < LiftConfig::NUM_MAGNETS; ++i) {
        digitalWrite(Pins::MAGNET_PINS[i], on ? HIGH : LOW);
    }
    magnet_state_ = on ? 0x1F : 0x00;
#endif
    if (on) {
        magnet_on_time_ms_ = millis();
    }
}

/* ============================================================================
 * STATUS QUERIES
 * ============================================================================ */

bool LiftController::isAtTop() const {
    if (top_switch_stable_ && bot_switch_stable_) return false; // Inconsistent/floating
    return top_switch_stable_;
}

bool LiftController::isAtBottom() const {
    if (top_switch_stable_ && bot_switch_stable_) return false; // Inconsistent/floating
    return bot_switch_stable_;
}

const char* LiftController::getStateString() const {
    switch (state_) {
    case LiftState::IDLE:     return "IDLE";
    case LiftState::RAISING:  return "RAISING";
    case LiftState::DROPPING: return "DROPPING";
    case LiftState::LOWERING: return "LOWERING";
    case LiftState::FAULT:    return "FAULT";
    default:                  return "UNKNOWN";
    }
}

/* ============================================================================
 * PRIVATE HELPERS
 * ============================================================================ */

void LiftController::driveMotor(bool up) {
    bool dir = LiftConfig::LIFT_INVERT_DIR ? !up : up;
    digitalWrite(Pins::LIFT_DIR, dir ? HIGH : LOW);
    analogWrite(Pins::LIFT_PWM, LiftConfig::LIFT_PWM_SPEED);
}

void LiftController::stopMotor() {
    analogWrite(Pins::LIFT_PWM, 0);
    digitalWrite(Pins::LIFT_PWM, LOW);
}

bool LiftController::readSwitch(uint8_t pin, unsigned long& last_change,
                                 bool& stable_state) const {
    bool current_reading = isSwitchRawActive(pin);

    if (current_reading != stable_state) {
        unsigned long now = millis();
        if (now - last_change >= LiftConfig::DEBOUNCE_MS) {
            stable_state = current_reading;
            last_change = now;
        }
    } else {
        last_change = millis();
    }

    return stable_state;
}

bool LiftController::isSwitchRawActive(uint8_t pin) const {
#if LIMIT_SWITCH_ACTIVE_HIGH
    return digitalRead(pin) == HIGH;
#else
    return digitalRead(pin) == LOW;
#endif
}
