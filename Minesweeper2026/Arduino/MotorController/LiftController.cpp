/**
 * ============================================================================
 * LIFTCONTROLLER.CPP - Lift Mechanism Implementation
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
      debounce_top_ms_(0),
      debounce_bot_ms_(0),
      top_switch_stable_(false),
      bot_switch_stable_(false) {}

/* ============================================================================
 * INITIALIZATION
 * ============================================================================ */

void LiftController::begin() {
    /* Configure lift motor pins */
    pinMode(Pins::LIFT_PWM, OUTPUT);
    pinMode(Pins::LIFT_DIR, OUTPUT);
    analogWrite(Pins::LIFT_PWM, 0);
    digitalWrite(Pins::LIFT_DIR, HIGH);

    /* Configure limit switch pins with internal pull-ups */
    pinMode(Pins::LIMIT_SW_TOP, INPUT_PULLUP);
    pinMode(Pins::LIMIT_SW_BOTTOM, INPUT_PULLUP);

    /* Configure electromagnet pins */
    for (uint8_t i = 0; i < LiftConfig::NUM_MAGNETS; ++i) {
        pinMode(Pins::MAGNET_PINS[i], OUTPUT);
        digitalWrite(Pins::MAGNET_PINS[i], LOW);
    }

    magnet_state_ = 0;
    state_ = LiftState::IDLE;

    /* Initialize debounce states */
    unsigned long now = millis();
    debounce_top_ms_ = now;
    debounce_bot_ms_ = now;
    top_switch_stable_ = (digitalRead(Pins::LIMIT_SW_TOP) == LOW);
    bot_switch_stable_ = (digitalRead(Pins::LIMIT_SW_BOTTOM) == LOW);
}

/* ============================================================================
 * STATE MACHINE UPDATE
 * ============================================================================ */

void LiftController::update() {
    /* Read debounced switch states */
    bool at_top = readSwitch(Pins::LIMIT_SW_TOP, debounce_top_ms_,
                             top_switch_stable_);
    bool at_bottom = readSwitch(Pins::LIMIT_SW_BOTTOM, debounce_bot_ms_,
                                bot_switch_stable_);

    switch (state_) {
    case LiftState::IDLE:
        /* No motor activity.  Waiting for raise/lower command. */
        break;

    case LiftState::RAISING:
        if (at_top) {
            /* Reached top limit switch — stop motor and transition. */
            stopMotor();
            state_ = LiftState::RAISED;
        } else if (millis() - motion_start_ms_ > LiftConfig::STALL_TIMEOUT_MS) {
            /* Stall timeout — motor running too long without hitting switch. */
            stopMotor();
            state_ = LiftState::FAULT;
        }
        break;

    case LiftState::RAISED:
        /* Lift is at top position.  Motor is off.  Waiting for lower command. */
        break;

    case LiftState::LOWERING:
        if (at_bottom) {
            /* Reached bottom limit switch — stop motor and transition. */
            stopMotor();
            state_ = LiftState::LOWERED;
        } else if (millis() - motion_start_ms_ > LiftConfig::STALL_TIMEOUT_MS) {
            /* Stall timeout. */
            stopMotor();
            state_ = LiftState::FAULT;
        }
        break;

    case LiftState::LOWERED:
        /* Lift is at bottom position.  Transition to IDLE. */
        state_ = LiftState::IDLE;
        break;

    case LiftState::FAULT:
        /* Motor is stopped.  Waiting for clearFault() call. */
        break;
    }
}

/* ============================================================================
 * COMMANDS
 * ============================================================================ */

void LiftController::raise() {
    if (state_ == LiftState::RAISING || state_ == LiftState::RAISED) {
        return;  /* Already at target or in progress */
    }
    if (state_ == LiftState::FAULT) {
        return;  /* Must clear fault first */
    }

    /* Check if already at top */
    if (isAtTop()) {
        state_ = LiftState::RAISED;
        return;
    }

    driveMotor(true);
    motion_start_ms_ = millis();
    state_ = LiftState::RAISING;
}

void LiftController::lower() {
    if (state_ == LiftState::LOWERING || state_ == LiftState::LOWERED) {
        return;  /* Already at target or in progress */
    }
    if (state_ == LiftState::FAULT) {
        return;  /* Must clear fault first */
    }

    /* Check if already at bottom */
    if (isAtBottom()) {
        state_ = LiftState::IDLE;
        return;
    }

    driveMotor(false);
    motion_start_ms_ = millis();
    state_ = LiftState::LOWERING;
}

void LiftController::stop() {
    stopMotor();
    if (state_ != LiftState::FAULT) {
        state_ = LiftState::IDLE;
    }
}

void LiftController::clearFault() {
    stopMotor();
    state_ = LiftState::IDLE;
}

/* ============================================================================
 * ELECTROMAGNET CONTROL
 * ============================================================================ */

void LiftController::setMagnet(uint8_t index, bool on) {
    if (index >= LiftConfig::NUM_MAGNETS) {
        return;
    }

    digitalWrite(Pins::MAGNET_PINS[index], on ? HIGH : LOW);

    if (on) {
        magnet_state_ |= (1U << index);
    } else {
        magnet_state_ &= ~(1U << index);
    }
}

void LiftController::setAllMagnets(bool on) {
    for (uint8_t i = 0; i < LiftConfig::NUM_MAGNETS; ++i) {
        digitalWrite(Pins::MAGNET_PINS[i], on ? HIGH : LOW);
    }
    magnet_state_ = on ? 0x1F : 0x00;
}

/* ============================================================================
 * STATUS QUERIES
 * ============================================================================ */

bool LiftController::isAtTop() const {
    return top_switch_stable_;
}

bool LiftController::isAtBottom() const {
    return bot_switch_stable_;
}

const char* LiftController::getStateString() const {
    switch (state_) {
    case LiftState::IDLE:     return "IDLE";
    case LiftState::RAISING:  return "RAISING";
    case LiftState::RAISED:   return "RAISED";
    case LiftState::LOWERING: return "LOWERING";
    case LiftState::LOWERED:  return "LOWERED";
    case LiftState::FAULT:    return "FAULT";
    default:                  return "UNKNOWN";
    }
}

/* ============================================================================
 * PRIVATE HELPERS
 * ============================================================================ */

void LiftController::driveMotor(bool up) {
    digitalWrite(Pins::LIFT_DIR, up ? HIGH : LOW);
    analogWrite(Pins::LIFT_PWM, LiftConfig::LIFT_PWM_SPEED);
}

void LiftController::stopMotor() {
    analogWrite(Pins::LIFT_PWM, 0);
}

bool LiftController::readSwitch(uint8_t pin, unsigned long& last_change,
                                 bool& stable_state) const {
    bool current_reading = (digitalRead(pin) == LOW);  /* Active LOW */

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
