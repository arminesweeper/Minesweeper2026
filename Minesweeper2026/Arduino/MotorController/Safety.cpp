/**
 * ============================================================================
 * SAFETY.CPP - Safety Systems Implementation
 * ============================================================================
 */

#include "Safety.h"
#include <avr/wdt.h>

SafetyMonitor::SafetyMonitor()
    : state_(SystemState::BOOTING), estop_trigger_time_(0) {
  faults_.command_timeout = false;
  faults_.encoder_right = false;
  faults_.encoder_left = false;
  faults_.battery_low = false;
  faults_.battery_high = false;
  faults_.velocity_limit = false;
  faults_.watchdog_reset = false;
  faults_.reserved = false;
}

void SafetyMonitor::begin() {
  // Check if we woke up from watchdog reset
  if (MCUSR & (1 << WDRF)) {
    faults_.watchdog_reset = true;
    MCUSR &= ~(1 << WDRF); // Clear the flag
  }

  // Disable watchdog initially
  wdt_disable();

#if ENABLE_WATCHDOG
  // Enable watchdog with 2-second timeout
  wdt_enable(WDTO_2S);
#endif

  setState(SystemState::ACTIVE);
  estop_trigger_time_ = millis();
}

SystemState SafetyMonitor::update(unsigned long last_command_time,
                                  double right_vel, double left_vel) {
  // Check for command timeout
  if (millis() - last_command_time > Safety::COMMAND_TIMEOUT_MS) {
    if (state_ == SystemState::ACTIVE) {
      faults_.command_timeout = true;
      setState(SystemState::ESTOP_TIMEOUT);
      if (DiagConfig::VERBOSE_SAFETY) {
        Serial.println(F("SAFETY: Command timeout"));
      }
    }
  }

  // Check for velocity sanity limits
  if (abs(right_vel) > Safety::MAX_VELOCITY_RAD_S * 1.5 ||
      abs(left_vel) > Safety::MAX_VELOCITY_RAD_S * 1.5) {
    faults_.velocity_limit = true;
  } else {
    faults_.velocity_limit = false;
  }

  // Check battery voltage
  checkBatteryVoltage();

  // Check for encoder faults
  if (faults_.encoder_right || faults_.encoder_left) {
    if (state_ == SystemState::ACTIVE) {
      setState(SystemState::FAULT_ENCODER);
    }
  }

  // Check battery faults
  if (faults_.battery_low || faults_.battery_high) {
    if (state_ == SystemState::ACTIVE) {
      setState(SystemState::FAULT_BATTERY);
    }
  }

  return state_;
}

void SafetyMonitor::triggerEStop() {
  setState(SystemState::ESTOP_MANUAL);
  estop_trigger_time_ = millis();

  if (DiagConfig::VERBOSE_SAFETY) {
    Serial.println(F("SAFETY: Manual E-Stop triggered"));
  }
}

void SafetyMonitor::clearEStop() {
  if (state_ == SystemState::ESTOP_MANUAL) {
    // Only clear if held for at least 100ms (debounce)
    if (millis() - estop_trigger_time_ > 100) {
      faults_.command_timeout = false;
      setState(SystemState::ACTIVE);
    }
  }
}

bool SafetyMonitor::hasFault() const {
  return faults_.command_timeout || faults_.encoder_right ||
         faults_.encoder_left || faults_.battery_low || faults_.battery_high ||
         faults_.velocity_limit;
}

void SafetyMonitor::setEncoderFault(bool is_right, bool fault) {
  if (is_right) {
    faults_.encoder_right = fault;
  } else {
    faults_.encoder_left = fault;
  }
}

uint16_t SafetyMonitor::readBatteryVoltage() const {
  uint16_t adc_value = analogRead(Pins::BATTERY_SENSE);
  // Convert ADC to millivolts
  // ADC range: 0-1023 maps to 0-5000mV (5V reference)
  uint16_t adc_millivolts = (uint32_t)adc_value * 5000UL / 1024UL;
  // Apply voltage divider ratio
  return (uint16_t)((uint32_t)adc_millivolts * Safety::VOLTAGE_DIVIDER_RATIO);
}

void SafetyMonitor::resetWatchdog() {
#if ENABLE_WATCHDOG
  wdt_reset();
#endif
}

void SafetyMonitor::setState(SystemState new_state) {
  if (new_state != state_) {
    if (DiagConfig::VERBOSE_SAFETY) {
      Serial.print(F("SAFETY: State "));
      Serial.print(getStateString());
      Serial.print(F(" -> "));
      state_ = new_state;
      Serial.println(getStateString());
    } else {
      state_ = new_state;
    }
  }
}

void SafetyMonitor::checkBatteryVoltage() {
  uint16_t voltage = readBatteryVoltage();

  if (voltage < Safety::BATTERY_VOLTAGE_LOW) {
    if (!faults_.battery_low && DiagConfig::VERBOSE_SAFETY) {
      Serial.print(F("SAFETY: Battery LOW "));
      Serial.print(voltage);
      Serial.println(F("mV"));
    }
    faults_.battery_low = true;
  } else {
    faults_.battery_low = false;
  }

  if (voltage > Safety::BATTERY_VOLTAGE_HIGH) {
    if (!faults_.battery_high && DiagConfig::VERBOSE_SAFETY) {
      Serial.print(F("SAFETY: Battery HIGH "));
      Serial.print(voltage);
      Serial.println(F("mV"));
    }
    faults_.battery_high = true;
  } else {
    faults_.battery_high = false;
  }
}

const char *SafetyMonitor::getStateString() const {
  switch (state_) {
  case SystemState::BOOTING:
    return "BOOTING";
  case SystemState::ACTIVE:
    return "ACTIVE";
  case SystemState::ESTOP_TIMEOUT:
    return "ESTOP_TIMEOUT";
  case SystemState::ESTOP_MANUAL:
    return "ESTOP_MANUAL";
  case SystemState::FAULT_ENCODER:
    return "FAULT_ENCODER";
  case SystemState::FAULT_BATTERY:
    return "FAULT_BATTERY";
  case SystemState::FAULT_UNKNOWN:
    return "FAULT_UNKNOWN";
  default:
    return "UNKNOWN";
  }
}