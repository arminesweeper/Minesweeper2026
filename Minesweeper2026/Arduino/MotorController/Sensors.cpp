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
    : metal_detected_(false),
      metal_raw_last_(false),
      metal_change_ms_(0),
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
 * INITIALIZATION
 * ============================================================================ */

void Sensors::begin() {
    /* Metal detector: digital input with internal pull-up (active LOW) */
    pinMode(Pins::METAL_DETECTOR, INPUT_PULLUP);

    /* Proximity sensors: analog inputs (no pinMode needed for analogRead) */

    /* Buzzer: output, initially off */
    pinMode(Pins::BUZZER, OUTPUT);
    noTone(Pins::BUZZER);

    /* Warning LED: output, initially off */
    pinMode(Pins::WARNING_LED, OUTPUT);
    digitalWrite(Pins::WARNING_LED, LOW);

    /* Initialize timing */
    unsigned long now = millis();
    metal_change_ms_ = now;
    buzzer_toggle_ms_ = now;
    led_toggle_ms_ = now;
}

/* ============================================================================
 * PERIODIC UPDATE
 * ============================================================================ */

void Sensors::update() {
    /* Read all proximity sensors */
    for (uint8_t i = 0; i < SensorConfig::NUM_PROXIMITY; ++i) {
        proximity_values_[i] = readProximityAveraged(Pins::PROXIMITY_PINS[i]);
    }

    /* Update metal detector (debounced) */
    updateMetalDetector();

    /* Update indicator outputs */
    updateBuzzer();
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

bool Sensors::isProximityTriggered(uint8_t index) const {
    if (index >= SensorConfig::NUM_PROXIMITY) {
        return false;
    }
    return proximity_values_[index] > SensorConfig::PROXIMITY_THRESHOLD;
}

uint16_t Sensors::readProximityAveraged(uint8_t pin) const {
    uint32_t sum = 0;
    for (uint8_t s = 0; s < SensorConfig::ADC_SAMPLES; ++s) {
        sum += static_cast<uint32_t>(analogRead(pin));
    }
    return static_cast<uint16_t>(sum / SensorConfig::ADC_SAMPLES);
}

/* ============================================================================
 * METAL DETECTOR
 * ============================================================================ */

void Sensors::updateMetalDetector() {
    bool current_reading = (digitalRead(Pins::METAL_DETECTOR) == LOW);

    if (current_reading != metal_raw_last_) {
        metal_change_ms_ = millis();
        metal_raw_last_ = current_reading;
    }

    if (millis() - metal_change_ms_ >= SensorConfig::METAL_DEBOUNCE_MS) {
        metal_detected_ = metal_raw_last_;
    }
}

/* ============================================================================
 * BUZZER PATTERN ENGINE
 * ============================================================================ */

void Sensors::setBuzzerPattern(BuzzerPattern pattern) {
    if (pattern == buzzer_pattern_) {
        return;
    }
    buzzer_pattern_ = pattern;
    buzzer_toggle_ms_ = millis();
    buzzer_on_ = false;

    /* Immediately silence if set to SILENT */
    if (pattern == BuzzerPattern::SILENT) {
        noTone(Pins::BUZZER);
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
            noTone(Pins::BUZZER);
            buzzer_on_ = false;
        }
        break;

    case BuzzerPattern::BEEP:
        if (!buzzer_on_) {
            tone(Pins::BUZZER, SensorConfig::BUZZER_FREQ_HZ);
            buzzer_on_ = true;
            buzzer_toggle_ms_ = now;
        } else if (elapsed >= 50) {
            /* Beep duration: 50 ms */
            noTone(Pins::BUZZER);
            buzzer_on_ = false;
            buzzer_pattern_ = BuzzerPattern::SILENT;
        }
        break;

    case BuzzerPattern::ALERT: {
        constexpr unsigned long ALERT_PERIOD = 200;
        if (elapsed >= ALERT_PERIOD) {
            buzzer_on_ = !buzzer_on_;
            if (buzzer_on_) {
                tone(Pins::BUZZER, SensorConfig::BUZZER_FREQ_HZ);
            } else {
                noTone(Pins::BUZZER);
            }
            buzzer_toggle_ms_ = now;
        }
        break;
    }

    case BuzzerPattern::ALARM: {
        constexpr unsigned long ALARM_PERIOD = 100;
        if (elapsed >= ALARM_PERIOD) {
            buzzer_on_ = !buzzer_on_;
            if (buzzer_on_) {
                tone(Pins::BUZZER, SensorConfig::BUZZER_FREQ_HZ * 2);
            } else {
                noTone(Pins::BUZZER);
            }
            buzzer_toggle_ms_ = now;
        }
        break;
    }

    case BuzzerPattern::MINE_DETECT:
        if (!buzzer_on_) {
            tone(Pins::BUZZER, SensorConfig::BUZZER_FREQ_HZ);
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
        /* Static off — no update needed */
        break;

    case LEDPattern::ON:
        /* Static on — no update needed */
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
        /*
         * Heartbeat pattern phases:
         *   Phase 0: ON  for 100 ms
         *   Phase 1: OFF for 100 ms
         *   Phase 2: ON  for 100 ms
         *   Phase 3: OFF for 600 ms
         */
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
