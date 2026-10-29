/**
 * ============================================================================
 * SENSORS.CPP - Peripheral Sensor Manager Implementation
 * ============================================================================
 * @file   Sensors.cpp
 * @author Assiut Robotics Team
 * @date   2026
 * ============================================================================
 */

#include "Sensors.h"

/* ============================================================================
 * CONSTRUCTOR
 * ============================================================================ */

Sensors::Sensors()
    : prox_debounced_(false),
      prox_candidate_(false),
      prox_candidate_samples_(0),
      prev_prox_for_edge_(false),
      prev_metal_for_edge_(false),
      proximity_enabled_(ENABLE_PROXIMITY == 1),
      ultrasonic_distance_cm_(SensorConfig::US_MAX_DISTANCE_CM),
      metal_detected_(false),
      metal_raw_last_(false),
      metal_change_ms_(0),
      siren_hold_until_ms_(0),
      siren_active_(false),
      buzzer_pattern_(BuzzerPattern::SILENT),
      buzzer_toggle_ms_(0),
      buzzer_on_(false),
      led_pattern_(LEDPattern::OFF),
      led_toggle_ms_(0),
      led_on_(false),
      heartbeat_phase_(0) {
    for (uint8_t i = 0; i < SensorConfig::NUM_PROXIMITY; ++i) {
        proximity_values_[i] = 0;
    }
}

/* ============================================================================
 * LOCAL HELPERS — buzzer / siren output abstraction
 * ============================================================================ */

static void alertOff() {
#if BUZZER_IS_SIREN_RELAY
#if SIREN_ACTIVE_HIGH
    digitalWrite(Pins::BUZZER, LOW);
#else
    digitalWrite(Pins::BUZZER, HIGH);
#endif
#else
    noTone(Pins::BUZZER);
#endif
}

static void alertOn(uint16_t freq_hz) {
#if ENABLE_SIREN
#if BUZZER_IS_SIREN_RELAY
    (void)freq_hz;
#if SIREN_ACTIVE_HIGH
    digitalWrite(Pins::BUZZER, HIGH);
#else
    digitalWrite(Pins::BUZZER, LOW);
#endif
#else
    tone(Pins::BUZZER, freq_hz);
#endif
#else
    (void)freq_hz;
    alertOff();
#endif
}

/* ============================================================================
 * INITIALIZATION
 * ============================================================================ */

void Sensors::begin() {
    /* Metal detector: digital input (Pin 33) */
#if ENABLE_METAL_DETECTOR
#if METAL_DETECTOR_ACTIVE_HIGH
    pinMode(Pins::METAL_DETECTOR, INPUT);
#else
    pinMode(Pins::METAL_DETECTOR, INPUT_PULLUP);
#endif
#endif

    /* Digital proximity sensors (direct digital inputs, active HIGH) */
    for (uint8_t i = 0; i < SensorConfig::NUM_PROXIMITY; ++i) {
        pinMode(Pins::PROXIMITY_PINS[i], INPUT);
    }

#if USE_ULTRASONIC_OBJECT_DETECT
    /* HC-SR04 Ultrasonic sensor */
    pinMode(Pins::US_TRIG, OUTPUT);
    digitalWrite(Pins::US_TRIG, LOW);
    pinMode(Pins::US_ECHO, INPUT);
#endif

    /* Buzzer / siren relay: output, initially off (Pin 37) */
    alertOff();
    pinMode(Pins::BUZZER, OUTPUT);
    alertOff();

    /* Warning LED: output, initially off */
    pinMode(Pins::WARNING_LED, OUTPUT);
    digitalWrite(Pins::WARNING_LED, LOW);

    /* Initialize timing */
    unsigned long now = millis();
    proximity_enabled_ = (ENABLE_PROXIMITY == 1);
    metal_change_ms_ = now;
    buzzer_toggle_ms_ = now;
    led_toggle_ms_ = now;
    siren_hold_until_ms_ = 0;
    prev_prox_for_edge_ = false;
    prev_metal_for_edge_ = false;
    metal_raw_last_ = (digitalRead(Pins::METAL_DETECTOR) == SensorConfig::METAL_ACTIVE_STATE);
    metal_detected_ = metal_raw_last_;
}

/* ============================================================================
 * PERIODIC UPDATE
 * ============================================================================ */

void Sensors::update() {
    /* Read digital proximity sensors if enabled */
    bool raw_prox = false;
    if (proximity_enabled_) {
        for (uint8_t i = 0; i < SensorConfig::NUM_PROXIMITY; ++i) {
            bool active = (digitalRead(Pins::PROXIMITY_PINS[i]) == SensorConfig::PROXIMITY_ACTIVE_STATE);
            proximity_values_[i] = active ? 1023 : 0;
            if (active) raw_prox = true;
        }

        /* Debounce aggregate proximity level over consecutive samples (matches RobotTest2) */
        if (raw_prox == prox_debounced_) {
            prox_candidate_ = raw_prox;
            prox_candidate_samples_ = 0;
        } else if (raw_prox != prox_candidate_) {
            prox_candidate_ = raw_prox;
            prox_candidate_samples_ = 1;
        } else if (prox_candidate_samples_ < 2) {
            ++prox_candidate_samples_;
        }
        if (prox_candidate_samples_ >= 2) {
            prox_debounced_ = prox_candidate_;
            prox_candidate_samples_ = 0;
        }
    } else {
        for (uint8_t i = 0; i < SensorConfig::NUM_PROXIMITY; ++i) {
            proximity_values_[i] = 0;
        }
        raw_prox = false;
        prox_debounced_ = false;
        prox_candidate_ = false;
        prox_candidate_samples_ = 0;
    }

    /* Update metal detector (debounced on Pin 33) */
    updateMetalDetector();

    /* Rising edge detection and siren one-shot hold logic (triggered on confirmed metal detection) */
    unsigned long now = millis();
    const bool prox_rise =
        proximity_enabled_ && prox_debounced_ && !prev_prox_for_edge_;

    const bool metal_rise = metal_detected_ && !prev_metal_for_edge_;
    if (metal_rise || prox_rise) {
        siren_hold_until_ms_ = now + SensorConfig::SIREN_HOLD_MS;
    }
    prev_prox_for_edge_ = proximity_enabled_ ? prox_debounced_ : false;
    prev_metal_for_edge_ = metal_detected_;

    const bool hold_active = (static_cast<int32_t>(siren_hold_until_ms_ - now) > 0);
    siren_active_ = (proximity_enabled_ && prox_debounced_) ||
                    metal_detected_ || hold_active;

    /* Drive siren hardware directly or via pattern engine */
    if (siren_active_) {
        alertOn(SensorConfig::BUZZER_FREQ_HZ);
        buzzer_on_ = true;
    } else if (buzzer_pattern_ == BuzzerPattern::SILENT) {
        alertOff();
        buzzer_on_ = false;
    } else {
        updateBuzzer();
    }

#if USE_ULTRASONIC_OBJECT_DETECT
    /* Read HC-SR04 ultrasonic distance every 60 ms (prevents sensor freeze/ringing) */
    static unsigned long last_us_time = 0;
    if (now - last_us_time >= 60) {
        last_us_time = now;
        uint16_t raw_dist = measureUltrasonicCm();
        if (raw_dist > 0 && raw_dist <= SensorConfig::US_MAX_DISTANCE_CM) {
            ultrasonic_distance_cm_ = raw_dist;
        }
    }
#endif

    updateLED();
}

/* ============================================================================
 * PROXIMITY SENSORS
 * ============================================================================ */

uint16_t Sensors::getProximity(uint8_t index) const {
    if (index >= SensorConfig::NUM_PROXIMITY) {
        return 0;
    }
    return proximity_values_[index];
}

void Sensors::getAllProximity(uint16_t* values) const {
    for (uint8_t i = 0; i < SensorConfig::NUM_PROXIMITY; ++i) {
        values[i] = proximity_values_[i];
    }
}

void Sensors::setProximityEnabled(bool enabled) {
    proximity_enabled_ = enabled;
    if (!proximity_enabled_) {
        for (uint8_t i = 0; i < SensorConfig::NUM_PROXIMITY; ++i) {
            proximity_values_[i] = 0;
        }
        prox_debounced_ = false;
        prox_candidate_ = false;
        prox_candidate_samples_ = 0;
    }
}

bool Sensors::isProximityTriggered(uint8_t index) const {
    if (!proximity_enabled_ || index >= SensorConfig::NUM_PROXIMITY) {
        return false;
    }
    return proximity_values_[index] > SensorConfig::PROXIMITY_THRESHOLD;
}

bool Sensors::isAnyProximityTriggered() const {
    return proximity_enabled_ && prox_debounced_;
}

uint16_t Sensors::readProximityAveraged(uint8_t pin) const {
    if (!proximity_enabled_) return 0;
    bool active = (digitalRead(pin) == SensorConfig::PROXIMITY_ACTIVE_STATE);
    return active ? 1023 : 0;
}

/* ============================================================================
 * ULTRASONIC DISTANCE MEASUREMENT
 * ============================================================================ */

uint16_t Sensors::measureUltrasonicCm() const {
    digitalWrite(Pins::US_TRIG, LOW);
    delayMicroseconds(4);
    digitalWrite(Pins::US_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(Pins::US_TRIG, LOW);

    // Bounded timeout matching SensorConfig::US_MAX_ECHO_US (prevents blocking cooperative loop)
    unsigned long duration = pulseIn(Pins::US_ECHO, HIGH, SensorConfig::US_MAX_ECHO_US);
    if (duration > 50 && duration < SensorConfig::US_MAX_ECHO_US) {
        uint16_t dist = static_cast<uint16_t>(duration / 58UL);
        return (dist > 0) ? dist : 1;
    }
    return SensorConfig::US_MAX_DISTANCE_CM;
}

/* ============================================================================
 * METAL DETECTOR
 * ============================================================================ */

void Sensors::updateMetalDetector() {
#if ENABLE_METAL_DETECTOR
    bool current_reading = (digitalRead(Pins::METAL_DETECTOR) == SensorConfig::METAL_ACTIVE_STATE);
    unsigned long now = millis();

    if (current_reading) {
        // Fast attack: immediately register detection on rising edge or active level
        metal_detected_ = true;
        metal_change_ms_ = now;
    } else {
        // Delayed decay: hold detection for debounce duration after signal drops
        if (now - metal_change_ms_ >= SensorConfig::METAL_DEBOUNCE_MS) {
            metal_detected_ = false;
        }
    }
    metal_raw_last_ = current_reading;
#else
    metal_detected_ = false;
#endif
}

/* ============================================================================
 * BUZZER / SIREN PATTERN ENGINE
 * ============================================================================ */

void Sensors::setBuzzerPattern(BuzzerPattern pattern) {
    if (pattern == buzzer_pattern_) {
        return;
    }
    buzzer_pattern_ = pattern;
    buzzer_toggle_ms_ = millis();
    buzzer_on_ = false;

    if (pattern == BuzzerPattern::SILENT) {
        alertOff();
    } else if (pattern == BuzzerPattern::MINE_DETECT || pattern == BuzzerPattern::ALERT || pattern == BuzzerPattern::ALARM) {
        alertOn(SensorConfig::BUZZER_FREQ_HZ);
        buzzer_on_ = true;
    }
}

void Sensors::beep() {
    setBuzzerPattern(BuzzerPattern::BEEP);
}

void Sensors::updateBuzzer() {
    unsigned long now = millis();
    unsigned long elapsed = now - buzzer_toggle_ms_;

    switch (buzzer_pattern_) {
    case BuzzerPattern::SILENT:
        if (buzzer_on_) {
            alertOff();
            buzzer_on_ = false;
        }
        break;

    case BuzzerPattern::BEEP:
        if (!buzzer_on_) {
            alertOn(SensorConfig::BUZZER_FREQ_HZ);
            buzzer_on_ = true;
            buzzer_toggle_ms_ = now;
        } else if (elapsed >= 50) {
            alertOff();
            buzzer_on_ = false;
            buzzer_pattern_ = BuzzerPattern::SILENT;
        }
        break;

    case BuzzerPattern::ALERT: {
#if BUZZER_IS_SIREN_RELAY
        if (!buzzer_on_) {
            alertOn(SensorConfig::BUZZER_FREQ_HZ);
            buzzer_on_ = true;
        }
#else
        constexpr unsigned long ALERT_PERIOD = 200;
        if (elapsed >= ALERT_PERIOD) {
            buzzer_on_ = !buzzer_on_;
            if (buzzer_on_) {
                alertOn(SensorConfig::BUZZER_FREQ_HZ);
            } else {
                alertOff();
            }
            buzzer_toggle_ms_ = now;
        }
#endif
        break;
    }

    case BuzzerPattern::ALARM: {
#if BUZZER_IS_SIREN_RELAY
        if (!buzzer_on_) {
            alertOn(SensorConfig::BUZZER_FREQ_HZ);
            buzzer_on_ = true;
        }
#else
        constexpr unsigned long ALARM_PERIOD = 100;
        if (elapsed >= ALARM_PERIOD) {
            buzzer_on_ = !buzzer_on_;
            if (buzzer_on_) {
                alertOn(static_cast<uint16_t>(SensorConfig::BUZZER_FREQ_HZ * 2));
            } else {
                alertOff();
            }
            buzzer_toggle_ms_ = now;
        }
#endif
        break;
    }

    case BuzzerPattern::MINE_DETECT:
        if (!buzzer_on_) {
            alertOn(SensorConfig::BUZZER_FREQ_HZ);
            buzzer_on_ = true;
        }
        break;
    }
}

/* ============================================================================
 * LED PATTERN ENGINE
 * ============================================================================ */

void Sensors::setLEDPattern(LEDPattern pattern) {
    if (pattern == led_pattern_) {
        return;
    }
    led_pattern_ = pattern;
    led_toggle_ms_ = millis();
    led_on_ = false;
    heartbeat_phase_ = 0;

    if (pattern == LEDPattern::OFF) {
        digitalWrite(Pins::WARNING_LED, LOW);
    } else if (pattern == LEDPattern::ON) {
        digitalWrite(Pins::WARNING_LED, HIGH);
        led_on_ = true;
    }
}

void Sensors::updateLED() {
    unsigned long now = millis();
    unsigned long elapsed = now - led_toggle_ms_;

    switch (led_pattern_) {
    case LEDPattern::OFF:
    case LEDPattern::ON:
        break;

    case LEDPattern::SLOW_BLINK: {
        constexpr unsigned long SLOW_PERIOD = 500;
        if (elapsed >= SLOW_PERIOD) {
            led_on_ = !led_on_;
            digitalWrite(Pins::WARNING_LED, led_on_ ? HIGH : LOW);
            led_toggle_ms_ = now;
        }
        break;
    }

    case LEDPattern::FAST_BLINK: {
        constexpr unsigned long FAST_PERIOD = 100;
        if (elapsed >= FAST_PERIOD) {
            led_on_ = !led_on_;
            digitalWrite(Pins::WARNING_LED, led_on_ ? HIGH : LOW);
            led_toggle_ms_ = now;
        }
        break;
    }

    case LEDPattern::HEARTBEAT: {
        unsigned long phase_duration;
        switch (heartbeat_phase_) {
        case 0: phase_duration = 100; break;
        case 1: phase_duration = 100; break;
        case 2: phase_duration = 100; break;
        case 3: phase_duration = 600; break;
        default: phase_duration = 600; break;
        }

        if (elapsed >= phase_duration) {
            heartbeat_phase_ = (heartbeat_phase_ + 1) % 4;
            led_on_ = (heartbeat_phase_ == 0 || heartbeat_phase_ == 2);
            digitalWrite(Pins::WARNING_LED, led_on_ ? HIGH : LOW);
            led_toggle_ms_ = now;
        }
        break;
    }
    }
}
