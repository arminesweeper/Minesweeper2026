/**
 * ============================================================================
 * SAFETY.CPP - Safety Systems Implementation
 * ============================================================================
 * @file   Safety.cpp
 * @author Assiut Robotics Team
 * @date   2026
 * ============================================================================
 */

#include "Safety.h"
#include <avr/wdt.h>

/* ============================================================================
 * CONSTRUCTOR
 * ============================================================================ */

SafetyMonitor::SafetyMonitor()
    : state_(SystemState::BOOTING), estop_trigger_time_(0) {
    faults_.command_timeout = false;
    faults_.encoder_right   = false;
    faults_.encoder_left    = false;
    faults_.battery_low     = false;
    faults_.battery_high    = false;
    faults_.velocity_limit  = false;
    faults_.watchdog_reset  = false;
    faults_.imu_fault       = false;
}

/* ============================================================================
 * INITIALIZATION
 * ============================================================================ */

void SafetyMonitor::begin() {
    /* Check if this boot was caused by a watchdog reset */
    if (MCUSR & (1 << WDRF)) {
        faults_.watchdog_reset = true;
        MCUSR &= ~(1 << WDRF);  /* Clear the watchdog reset flag */
    }

    /* Disable watchdog initially (re-enable below if configured) */
    wdt_disable();

#if ENABLE_WATCHDOG
    /* Enable hardware watchdog with 2-second timeout */
    wdt_enable(WDTO_2S);
#endif

    setState(SystemState::ACTIVE);
    estop_trigger_time_ = millis();
}

/* ============================================================================
 * PERIODIC SAFETY CHECK
 * ============================================================================ */

SystemState SafetyMonitor::update(unsigned long last_command_time,
                                   double right_vel, double left_vel) {
    /* --- Check for serial command timeout --- */
    if (millis() - last_command_time > SafetyConfig::COMMAND_TIMEOUT_MS) {
        if (state_ == SystemState::ACTIVE) {
            faults_.command_timeout = true;
            setState(SystemState::ESTOP_TIMEOUT);
            if (DiagConfig::VERBOSE_SAFETY) {
                Serial.println(F("SAFETY: Command timeout"));
            }
        }
    }

    /* --- Check measured velocity sanity limits --- */
    if (abs(right_vel) > SafetyConfig::MAX_VELOCITY_RAD_S * 1.5 ||
        abs(left_vel) > SafetyConfig::MAX_VELOCITY_RAD_S * 1.5) {
        faults_.velocity_limit = true;
    } else {
        faults_.velocity_limit = false;
    }

    /* --- Check battery voltage --- */
    checkBatteryVoltage();

    /* --- Check encoder faults --- */
    if (faults_.encoder_right || faults_.encoder_left) {
        if (state_ == SystemState::ACTIVE) {
            setState(SystemState::FAULT_ENCODER);
        }
    }

    /* --- Check IMU fault --- */
    if (faults_.imu_fault) {
        if (state_ == SystemState::ACTIVE) {
            setState(SystemState::FAULT_IMU);
        }
    }

    /* --- Check battery faults --- */
    if (faults_.battery_low || faults_.battery_high) {
        if (state_ == SystemState::ACTIVE) {
            setState(SystemState::FAULT_BATTERY);
        }
    }

    return state_;
}

/* ============================================================================
 * EMERGENCY STOP
 * ============================================================================ */

void SafetyMonitor::triggerEStop() {
    setState(SystemState::ESTOP_MANUAL);
    estop_trigger_time_ = millis();

    if (DiagConfig::VERBOSE_SAFETY) {
        Serial.println(F("SAFETY: Manual E-Stop triggered"));
    }
}

void SafetyMonitor::clearEStop() {
    if (state_ == SystemState::ESTOP_TIMEOUT) {
        /* Timeout clears immediately when a new command arrives */
        faults_.command_timeout = false;
        setState(SystemState::ACTIVE);
    } else if (state_ == SystemState::ESTOP_MANUAL) {
        /* Manual E-Stop requires debounce (100 ms minimum) */
        if (millis() - estop_trigger_time_ > 100) {
            faults_.command_timeout = false;
            setState(SystemState::ACTIVE);
        }
    }
}

/* ============================================================================
 * FAULT MANAGEMENT
 * ============================================================================ */

bool SafetyMonitor::hasFault() const {
    return faults_.command_timeout || faults_.encoder_right ||
           faults_.encoder_left || faults_.battery_low ||
           faults_.battery_high || faults_.velocity_limit ||
           faults_.imu_fault;
}

void SafetyMonitor::setEncoderFault(bool is_right, bool fault) {
    if (is_right) {
        faults_.encoder_right = fault;
    } else {
        faults_.encoder_left = fault;
    }

    /* Auto-clear encoder fault state if both faults are resolved */
    if (!faults_.encoder_right && !faults_.encoder_left) {
        if (state_ == SystemState::FAULT_ENCODER) {
            setState(SystemState::ACTIVE);
        }
    }
}

void SafetyMonitor::setIMUFault(bool fault) {
    faults_.imu_fault = fault;

    if (!fault && state_ == SystemState::FAULT_IMU) {
        setState(SystemState::ACTIVE);
    }
}

void SafetyMonitor::setLiftFault(bool fault) {
    if (fault) {
        if (state_ == SystemState::ACTIVE) {
            setState(SystemState::FAULT_LIFT);
        }
    } else {
        if (state_ == SystemState::FAULT_LIFT) {
            setState(SystemState::ACTIVE);
        }
    }
}

/* ============================================================================
 * BATTERY MONITORING
 * ============================================================================ */

uint16_t SafetyMonitor::readBatteryVoltage() const {
    uint16_t adc_value = analogRead(Pins::BATTERY_SENSE);
    /* ADC: 0-1023 maps to 0-5000 mV (5V reference) */
    uint16_t adc_millivolts = static_cast<uint16_t>(
        static_cast<uint32_t>(adc_value) * 5000UL / 1024UL);
    /* Apply voltage divider ratio to get actual battery voltage */
    return static_cast<uint16_t>(
        static_cast<uint32_t>(adc_millivolts) *
        static_cast<uint32_t>(SafetyConfig::VOLTAGE_DIVIDER_RATIO));
}

void SafetyMonitor::checkBatteryVoltage() {
    uint16_t voltage = readBatteryVoltage();

    if (voltage < SafetyConfig::BATTERY_VOLTAGE_LOW) {
        if (!faults_.battery_low && DiagConfig::VERBOSE_SAFETY) {
            Serial.print(F("SAFETY: Battery LOW "));
            Serial.print(voltage);
            Serial.println(F("mV"));
        }
        faults_.battery_low = true;
    } else {
        faults_.battery_low = false;
    }

    if (voltage > SafetyConfig::BATTERY_VOLTAGE_HIGH) {
        if (!faults_.battery_high && DiagConfig::VERBOSE_SAFETY) {
            Serial.print(F("SAFETY: Battery HIGH "));
            Serial.print(voltage);
            Serial.println(F("mV"));
        }
        faults_.battery_high = true;
    } else {
        faults_.battery_high = false;
    }

    /* Auto-clear battery fault state if voltage returns to safe range */
    if (!faults_.battery_low && !faults_.battery_high) {
        if (state_ == SystemState::FAULT_BATTERY) {
            setState(SystemState::ACTIVE);
        }
    }
}

/* ============================================================================
 * WATCHDOG
 * ============================================================================ */

void SafetyMonitor::resetWatchdog() {
#if ENABLE_WATCHDOG
    wdt_reset();
#endif
}

/* ============================================================================
 * STATE MANAGEMENT
 * ============================================================================ */

void SafetyMonitor::setState(SystemState new_state) {
    if (new_state != state_) {
        if (DiagConfig::VERBOSE_SAFETY) {
            Serial.print(F("SAFETY: State "));
            Serial.print(getStateString());
            Serial.print(F(" -> "));
        }
        state_ = new_state;
        if (DiagConfig::VERBOSE_SAFETY) {
            Serial.println(getStateString());
        }
    }
}

const char* SafetyMonitor::getStateString() const {
    switch (state_) {
    case SystemState::BOOTING:        return "BOOTING";
    case SystemState::ACTIVE:         return "ACTIVE";
    case SystemState::ESTOP_TIMEOUT:  return "ESTOP_TIMEOUT";
    case SystemState::ESTOP_MANUAL:   return "ESTOP_MANUAL";
    case SystemState::FAULT_ENCODER:  return "FAULT_ENCODER";
    case SystemState::FAULT_BATTERY:  return "FAULT_BATTERY";
    case SystemState::FAULT_IMU:      return "FAULT_IMU";
    case SystemState::FAULT_LIFT:     return "FAULT_LIFT";
    case SystemState::FAULT_UNKNOWN:  return "FAULT_UNKNOWN";
    default:                          return "UNKNOWN";
    }
}