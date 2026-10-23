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
      magnet_hold_until_ms_(0),
      debounce_top_ms_(0),
      debounce_bot_ms_(0),
      debounce_sensor_ms_(0),
      top_switch_stable_(false),
      bot_switch_stable_(false),
      sensor_stable_(false),
      sensor_prev_state_(false),
      manual_trigger_(false),
      manual_magnet_override_(false),
      is_auto_cycling_(false),
      manual_active_(false),
      manual_joystick_active_(false),
      motor_running_(false),
      motor_dir_up_(false) {}

/* ============================================================================
 * INITIALIZATION
 * ============================================================================ */

void LiftController::begin() {
    /* Configure lift motor pins */
    pinMode(Pins::LIFT_PWM, OUTPUT);
    pinMode(Pins::LIFT_DIR, OUTPUT);
    digitalWrite(Pins::LIFT_PWM, LOW);
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
    digitalWrite(Pins::MAGNET_RELAY, LOW); // Start OFF
#else
    for (uint8_t i = 0; i < LiftConfig::NUM_MAGNETS; ++i) {
        pinMode(Pins::MAGNET_PINS[i], OUTPUT);
        digitalWrite(Pins::MAGNET_PINS[i], LOW); // Start OFF
    }
#endif

    /* Magnets start OFF to save battery and prevent coil heating */
    setAllMagnets(false);
    state_ = LiftState::IDLE;
    manual_trigger_ = false;
    manual_magnet_override_ = false;
    is_auto_cycling_ = false;
    manual_active_ = false;
    manual_joystick_active_ = false;
    motor_running_ = false;
    motor_dir_up_ = false;
    magnet_hold_until_ms_ = 0;

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
    /* Maintain PWM output while motor is running */
    applyPWM();

    const unsigned long now = millis();
    const bool mine_detected = metal_detected || object_detected;

    /* Electromagnet control: energize immediately whenever mine is detected */
    if (mine_detected) {
        setAllMagnets(true);
        magnet_hold_until_ms_ = now + LiftConfig::MAGNET_ARM_TIMEOUT_MS;
    } else if (manual_magnet_override_) {
        setAllMagnets(true);
    } else if (static_cast<int32_t>(magnet_hold_until_ms_ - now) <= 0) {
        /* De-energize magnets once hold duration expires to prevent coil heating */
        if (isMagnetActive() && state_ == LiftState::IDLE) {
            setAllMagnets(false);
        }
    }

#if ENABLE_LIMIT_SWITCHES
    /* Read debounced switch states only when limit switches are enabled */
    bool at_top = readSwitch(Pins::LIMIT_SW_TOP, debounce_top_ms_, top_switch_stable_);
    bool at_bottom = readSwitch(Pins::LIMIT_SW_BOTTOM, debounce_bot_ms_, bot_switch_stable_);
    bool raw_top = isSwitchRawActive(Pins::LIMIT_SW_TOP);
    bool raw_bottom = isSwitchRawActive(Pins::LIMIT_SW_BOTTOM);
#endif

    sensor_stable_ = mine_detected;
    sensor_prev_state_ = mine_detected;

    switch (state_) {
    case LiftState::IDLE:
#if ENABLE_LIFT_AUTO_CYCLES
        /* Automated cycle trigger: when an object enters detection range or manual trigger */
        if (manual_trigger_ || (object_detected && !sensor_prev_state_)) {
            manual_trigger_ = false;
            is_auto_cycling_ = true;
            manual_active_ = false;
            setAllMagnets(true);
            driveMotor(true);
            motion_start_ms_ = now;
            state_ = LiftState::RAISING;
        } else {
            stopMotor();
        }
#else
        stopMotor();
#endif
        break;

    case LiftState::RAISING:
#if ENABLE_LIFT_AUTO_CYCLES
        if (is_auto_cycling_) {
#if ENABLE_LIMIT_SWITCHES
            if (raw_top || at_top || (now - motion_start_ms_ > LiftConfig::STALL_TIMEOUT_MS)) {
                stopMotor();
                if (!manual_magnet_override_) {
                    setAllMagnets(false);
                }
                dwell_start_ms_ = now;
                state_ = LiftState::DROPPING;
                break;
            }
#else
            if (now - motion_start_ms_ > LiftConfig::STALL_TIMEOUT_MS) {
                stopMotor();
                if (!manual_magnet_override_) {
                    setAllMagnets(false);
                }
                dwell_start_ms_ = now;
                state_ = LiftState::DROPPING;
                break;
            }
#endif
        }
#endif
#if ENABLE_LIMIT_SWITCHES
        if (raw_top || at_top) {
            stop();
            break;
        }
#endif
        /* Safety watchdog timeout in case operator leaves motor running */
        if (now - motion_start_ms_ > LiftConfig::MANUAL_MOVE_TIMEOUT_MS) {
            stop();
        }
        break;

    case LiftState::DROPPING:
#if ENABLE_LIFT_AUTO_CYCLES
        if (is_auto_cycling_) {
            if (now - dwell_start_ms_ >= LiftConfig::DROP_DWELL_MS) {
                if (!manual_magnet_override_) {
                    setAllMagnets(false);
                }
                driveMotor(false);
                motion_start_ms_ = now;
                state_ = LiftState::LOWERING;
            }
            break;
        }
#endif
        stopMotor();
        state_ = LiftState::IDLE;
        break;

    case LiftState::LOWERING:
#if ENABLE_LIFT_AUTO_CYCLES
        if (is_auto_cycling_) {
#if ENABLE_LIMIT_SWITCHES
            if (raw_bottom || at_bottom || (now - motion_start_ms_ > LiftConfig::STALL_TIMEOUT_MS)) {
                stopMotor();
                if (!manual_magnet_override_) {
                    setAllMagnets(false);
                }
                is_auto_cycling_ = false;
                state_ = LiftState::IDLE;
                break;
            }
#else
            if (now - motion_start_ms_ > LiftConfig::STALL_TIMEOUT_MS) {
                stopMotor();
                if (!manual_magnet_override_) {
                    setAllMagnets(false);
                }
                is_auto_cycling_ = false;
                state_ = LiftState::IDLE;
                break;
            }
#endif
        }
#endif
#if ENABLE_LIMIT_SWITCHES
        if (raw_bottom || at_bottom) {
            stop();
            break;
        }
#endif
        /* Safety watchdog timeout in case operator leaves motor running */
        if (now - motion_start_ms_ > LiftConfig::MANUAL_MOVE_TIMEOUT_MS) {
            stop();
        }
        break;

    case LiftState::FAULT:
        stopMotor();
        // Latch in FAULT until clearFault() or manual reset is called
        break;
    }
}

/* ============================================================================
 * COMMANDS — Fully Manual Direct Motor Control
 * ============================================================================ */

void LiftController::triggerCycle() {
#if ENABLE_LIFT_AUTO_CYCLES
    clearFault();
    manual_trigger_ = true;
    is_auto_cycling_ = true;
    manual_active_ = false;
    setAllMagnets(true);
    driveMotor(true);
    motion_start_ms_ = millis();
    state_ = LiftState::RAISING;
#else
    // All automated cycles are disabled in Config.h (ENABLE_LIFT_AUTO_CYCLES = 0)
    // Gripper is in pure manual mode; ignore automatic cycle requests
    stop();
#endif
}

void LiftController::raise() {
    clearFault();
    manual_active_ = true;
    is_auto_cycling_ = false;
    driveMotor(true); /* Move UP smoothly at configured PWM */
    motion_start_ms_ = millis();
    state_ = LiftState::RAISING;
}

void LiftController::lower() {
    clearFault();
    manual_active_ = true;
    is_auto_cycling_ = false;
    driveMotor(false); /* Move DOWN smoothly at configured PWM */
    motion_start_ms_ = millis();
    state_ = LiftState::LOWERING;
}

void LiftController::stop() {
    stopMotor();
    is_auto_cycling_ = false;
    manual_active_ = false;
    manual_joystick_active_ = false;
    state_ = LiftState::IDLE;
}

void LiftController::handleVelocityCommand(float g_vel) {
    if (g_vel > 0.1f) {
        clearFault();
        manual_joystick_active_ = true;
        manual_active_ = true;
        is_auto_cycling_ = false;
        driveMotor(true);
        motion_start_ms_ = millis();
        state_ = LiftState::RAISING;
    } else if (g_vel < -0.1f) {
        clearFault();
        manual_joystick_active_ = true;
        manual_active_ = true;
        is_auto_cycling_ = false;
        driveMotor(false);
        motion_start_ms_ = millis();
        state_ = LiftState::LOWERING;
    } else {
        if (manual_joystick_active_) {
            manual_joystick_active_ = false;
            stop();
        }
    }
}

void LiftController::clearFault() {
    stopMotor();
    state_ = LiftState::IDLE;
    manual_trigger_ = false;
    is_auto_cycling_ = false;
    manual_active_ = false;
    manual_joystick_active_ = false;
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
#if ENABLE_LIMIT_SWITCHES
    return isSwitchRawActive(Pins::LIMIT_SW_TOP) || top_switch_stable_;
#else
    return false;
#endif
}

bool LiftController::isAtBottom() const {
#if ENABLE_LIMIT_SWITCHES
    return isSwitchRawActive(Pins::LIMIT_SW_BOTTOM) || bot_switch_stable_;
#else
    return false;
#endif
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
 * PRIVATE HELPERS & MOTOR PWM
 * ============================================================================ */

void LiftController::driveMotor(bool up) {
#if ENABLE_LIMIT_SWITCHES
    // Safety check: refuse to drive into an already-active limit switch
    if (up && (isSwitchRawActive(Pins::LIMIT_SW_TOP) || top_switch_stable_)) {
        stopMotor();
        return;
    }
    if (!up && (isSwitchRawActive(Pins::LIMIT_SW_BOTTOM) || bot_switch_stable_)) {
        stopMotor();
        return;
    }
#endif

    bool dir = LiftConfig::LIFT_INVERT_DIR ? !up : up;
    digitalWrite(Pins::LIFT_DIR, dir ? HIGH : LOW);
    motor_running_ = true;
    motor_dir_up_ = up;
    applyPWM();
}

void LiftController::stopMotor() {
    motor_running_ = false;
    analogWrite(Pins::LIFT_PWM, 0);
    digitalWrite(Pins::LIFT_PWM, LOW);
}

void LiftController::applyPWM() {
    if (!motor_running_) {
        digitalWrite(Pins::LIFT_PWM, LOW);
        analogWrite(Pins::LIFT_PWM, 0);
        return;
    }

#if ENABLE_LIMIT_SWITCHES
    // Instant safety cutoff during movement
    if (motor_dir_up_ && isSwitchRawActive(Pins::LIMIT_SW_TOP)) {
        stopMotor();
        return;
    }
    if (!motor_dir_up_ && isSwitchRawActive(Pins::LIMIT_SW_BOTTOM)) {
        stopMotor();
        return;
    }
#endif

    // Hardware PWM for pins supported by AVR hardware timers (e.g., Pin 7 on Timer 4 OC4B)
    if (digitalPinToTimer(Pins::LIFT_PWM) != NOT_ON_TIMER) {
        analogWrite(Pins::LIFT_PWM, LiftConfig::LIFT_PWM_SPEED);
        return;
    }

    // Software PWM modulation for non-timer digital pins (e.g., Pin 49)
    // 20 ms cycle (50 Hz). ON time = (speed * 20) / 255 ms
    uint32_t phase = millis() % 20;
    uint32_t on_ms = ((uint32_t)LiftConfig::LIFT_PWM_SPEED * 20) / 255;
    if (on_ms < 1) on_ms = 1;
    digitalWrite(Pins::LIFT_PWM, (phase < on_ms) ? HIGH : LOW);
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
