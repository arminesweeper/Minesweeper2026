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
      manual_magnet_override_(false),
      is_auto_cycling_(false),
      manual_active_(false),
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
    motor_running_ = false;
    motor_dir_up_ = false;

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

    /* Read debounced switch states */
    bool at_top = readSwitch(Pins::LIMIT_SW_TOP, debounce_top_ms_, top_switch_stable_);
    bool at_bottom = readSwitch(Pins::LIMIT_SW_BOTTOM, debounce_bot_ms_, bot_switch_stable_);

    /* Instantaneous zero-latency limit switch checks to protect microswitches */
    bool raw_top = isSwitchRawActive(Pins::LIMIT_SW_TOP);
    bool raw_bottom = isSwitchRawActive(Pins::LIMIT_SW_BOTTOM);

    sensor_stable_ = object_detected;

    switch (state_) {
    case LiftState::IDLE: {
        /* Keep magnets OFF in IDLE unless operator manually energized them */
        if (!manual_magnet_override_ && isMagnetActive()) {
            setAllMagnets(false);
        }

        /* Trigger cycle if an object/mine newly enters ultrasonic detection range,
         * OR if triggered manually via command. */
        bool new_object = (object_detected && !sensor_prev_state_);
        sensor_prev_state_ = object_detected;

        if (new_object || manual_trigger_ || (object_detected && !at_top && (millis() - motion_start_ms_ > 2000))) {
            manual_trigger_ = false;
            is_auto_cycling_ = true;
            manual_active_ = false;
            setAllMagnets(true); /* Energize magnet to grab mine */
            driveMotor(true);    /* Move UP smoothly */
            motion_start_ms_ = millis();
            state_ = LiftState::RAISING;
        }
        break;
    }

    case LiftState::RAISING:
        sensor_prev_state_ = object_detected;
        /* Instant cutoff when hitting top switch (0 ms latency) or travel timeout */
        if (raw_top || at_top || (millis() - motion_start_ms_ > LiftConfig::STALL_TIMEOUT_MS)) {
            stopMotor();
            if (!manual_magnet_override_) {
                setAllMagnets(false); /* De-energize magnet to drop mine into container */
            }
            dwell_start_ms_ = millis();
            state_ = LiftState::DROPPING;
        }
        break;

    case LiftState::DROPPING:
        sensor_prev_state_ = object_detected;
        /* Wait for dwell with magnet OFF so mine falls clear into container */
        if (millis() - dwell_start_ms_ >= LiftConfig::DROP_DWELL_MS) {
            /* Magnets stay OFF while descending to save power */
            if (!manual_magnet_override_) {
                setAllMagnets(false);
            }
            driveMotor(false); /* Move DOWN smoothly */
            motion_start_ms_ = millis();
            state_ = LiftState::LOWERING;
        }
        break;

    case LiftState::LOWERING:
        sensor_prev_state_ = object_detected;
        /* Instant cutoff when hitting bottom switch (0 ms latency) or travel timeout */
        if (raw_bottom || at_bottom || (millis() - motion_start_ms_ > LiftConfig::STALL_TIMEOUT_MS)) {
            stopMotor();
            /* Keep magnets OFF in IDLE */
            if (!manual_magnet_override_) {
                setAllMagnets(false);
            }
            is_auto_cycling_ = false;
            state_ = LiftState::IDLE;
        }
        break;

    case LiftState::FAULT:
        sensor_prev_state_ = object_detected;
        stopMotor();
        if (!manual_magnet_override_) {
            setAllMagnets(false);
        }
        is_auto_cycling_ = false;
        manual_active_ = false;
        break;
    }
}

/* ============================================================================
 * COMMANDS
 * ============================================================================ */

void LiftController::triggerCycle() {
    clearFault();
    manual_trigger_ = true;
    is_auto_cycling_ = true;
    manual_active_ = false;
    setAllMagnets(true); // Energize to pick up mine
    driveMotor(true);
    motion_start_ms_ = millis();
    state_ = LiftState::RAISING;
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
    is_auto_cycling_ = false;
    manual_active_ = false;
    state_ = LiftState::IDLE;
}

void LiftController::handleVelocityCommand(float g_vel) {
    if (is_auto_cycling_) {
        // Automated cycle in progress: ignore neutral velocity commands
        if (fabs(g_vel) <= 0.1f) {
            return;
        }
        // Active operator command cancels automatic cycle and engages manual driving
        is_auto_cycling_ = false;
    }

    if (g_vel > 0.1f) {
        // Manual raise UP
        if (state_ == LiftState::FAULT) clearFault();
        manual_active_ = true;
        setAllMagnets(true);
        if (!isAtTop()) {
            driveMotor(true);
            motion_start_ms_ = millis();
            state_ = LiftState::RAISING;
        } else {
            stopMotor();
        }
    } else if (g_vel < -0.1f) {
        // Manual lower DOWN
        if (state_ == LiftState::FAULT) clearFault();
        manual_active_ = true;
        setAllMagnets(true);
        if (!isAtBottom()) {
            driveMotor(false);
            motion_start_ms_ = millis();
            state_ = LiftState::LOWERING;
        } else {
            stopMotor();
        }
    } else {
        // Neutral joystick: only stop if currently driving manually
        if (manual_active_) {
            stopMotor();
            manual_active_ = false;
            state_ = LiftState::IDLE;
        }
    }
}

void LiftController::clearFault() {
    stopMotor();
    state_ = LiftState::IDLE;
    manual_trigger_ = false;
    is_auto_cycling_ = false;
    manual_active_ = false;
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
    return isSwitchRawActive(Pins::LIMIT_SW_TOP) || top_switch_stable_;
}

bool LiftController::isAtBottom() const {
    return isSwitchRawActive(Pins::LIMIT_SW_BOTTOM) || bot_switch_stable_;
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
    // Safety check: refuse to drive into an already-active limit switch
    if (up && (isSwitchRawActive(Pins::LIMIT_SW_TOP) || top_switch_stable_)) {
        stopMotor();
        return;
    }
    if (!up && (isSwitchRawActive(Pins::LIMIT_SW_BOTTOM) || bot_switch_stable_)) {
        stopMotor();
        return;
    }

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

    // Instant safety cutoff during movement
    if (motor_dir_up_ && isSwitchRawActive(Pins::LIMIT_SW_TOP)) {
        stopMotor();
        return;
    }
    if (!motor_dir_up_ && isSwitchRawActive(Pins::LIMIT_SW_BOTTOM)) {
        stopMotor();
        return;
    }

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
