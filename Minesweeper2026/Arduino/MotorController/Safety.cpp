/**
 * ============================================================================
 * SAFETY.CPP - Safety Systems Implementation
 * ============================================================================
 */

#include "Safety.h"
#include <avr/wdt.h>

SafetyMonitor::SafetyMonitor()
    : state_(SystemState::BOOT), estop_trigger_time_(0), 
      filtered_battery_mv_(0.0f), battery_initialized_(false),
      stall_timer_r_(0), stall_timer_l_(0) {
    memset(&faults_, 0, sizeof(FaultFlags));
}

void SafetyMonitor::begin() {
    if (MCUSR & (1 << WDRF)) {
        faults_.watchdog_reset = true;
        MCUSR &= ~(1 << WDRF); 
    }
    wdt_disable();
#if ENABLE_WATCHDOG
    wdt_enable(WDTO_2S);
#endif
    state_ = SystemState::INIT;
}

SystemState SafetyMonitor::update(unsigned long last_command_time, 
                                  float right_vel, float left_vel,
                                  float right_pwm, float left_pwm) {
    
    // 1. Check Command Timeout (only applies if we are RUNNING or READY)
    if (state_ == SystemState::RUNNING || state_ == SystemState::READY) {
        if (millis() - last_command_time > SafetyConfig::COMMAND_TIMEOUT_MS) {
            faults_.command_timeout = true;
            forceState(SystemState::ESTOP);
        } else {
            faults_.command_timeout = false;
        }
    }

    // 2. Battery Monitoring
    checkBatteryVoltage();

    // 3. Motor Stall Detection
    checkMotorStall(right_vel, left_vel, right_pwm, left_pwm);

    // 4. Fault Aggregation
    if (hasFault()) {
        if (state_ == SystemState::RUNNING || state_ == SystemState::READY || state_ == SystemState::PAUSED) {
            forceState(SystemState::FAULT);
        }
    } else {
        // If we were in FAULT but faults cleared, transition back to READY
        if (state_ == SystemState::FAULT) {
            forceState(SystemState::READY);
        }
    }

    // Critical battery check forces shutdown
    if (faults_.battery_critical) {
        forceState(SystemState::SHUTDOWN);
    }

    return state_;
}

void SafetyMonitor::checkMotorStall(float right_vel, float left_vel, float right_pwm, float left_pwm) {
    // Thresholds
    const float STALL_PWM_THRESHOLD = 100.0f;
    const float STALL_VEL_THRESHOLD = 0.05f;
    const unsigned long STALL_TIME_MS = 500;

    // Right Wheel
    if (abs(right_pwm) > STALL_PWM_THRESHOLD && abs(right_vel) < STALL_VEL_THRESHOLD) {
        if (stall_timer_r_ == 0) stall_timer_r_ = millis();
        else if (millis() - stall_timer_r_ > STALL_TIME_MS) faults_.motor_stall_r = true;
    } else {
        stall_timer_r_ = 0;
    }

    // Left Wheel
    if (abs(left_pwm) > STALL_PWM_THRESHOLD && abs(left_vel) < STALL_VEL_THRESHOLD) {
        if (stall_timer_l_ == 0) stall_timer_l_ = millis();
        else if (millis() - stall_timer_l_ > STALL_TIME_MS) faults_.motor_stall_l = true;
    } else {
        stall_timer_l_ = 0;
    }
}

float SafetyMonitor::readBatteryVoltage() {
    uint16_t adc_value = analogRead(Pins::BATTERY_SENSE);
    float current_mv = (static_cast<float>(adc_value) * 5000.0f / 1024.0f) * SafetyConfig::VOLTAGE_DIVIDER_RATIO;
    
    if (!battery_initialized_) {
        filtered_battery_mv_ = current_mv;
        battery_initialized_ = true;
    } else {
        // Exponential moving average filter
        filtered_battery_mv_ = (0.95f * filtered_battery_mv_) + (0.05f * current_mv);
    }
    
    return filtered_battery_mv_;
}

void SafetyMonitor::checkBatteryVoltage() {
    float voltage = readBatteryVoltage();

    if (voltage < SafetyConfig::BATTERY_VOLTAGE_CRITICAL) {
        faults_.battery_critical = true;
        faults_.battery_low = true;
    } else if (voltage < SafetyConfig::BATTERY_VOLTAGE_LOW) {
        faults_.battery_low = true;
        faults_.battery_critical = false;
    } else {
        faults_.battery_low = false;
        faults_.battery_critical = false;
    }
}

void SafetyMonitor::triggerEStop() {
    forceState(SystemState::ESTOP);
    estop_trigger_time_ = millis();
}

void SafetyMonitor::clearEStop() {
    if (state_ == SystemState::ESTOP) {
        if (millis() - estop_trigger_time_ > 100) {
            if (!hasFault()) forceState(SystemState::READY);
            else forceState(SystemState::FAULT);
        }
    }
}

void SafetyMonitor::triggerFault(const char* reason) {
    (void)reason; // Could be logged
    forceState(SystemState::FAULT);
}

void SafetyMonitor::clearFaults() {
    faults_.motor_stall_r = false;
    faults_.motor_stall_l = false;
    faults_.encoder_right = false;
    faults_.encoder_left = false;
    faults_.imu_fault = false;
    faults_.lift_fault = false;
    stall_timer_r_ = 0;
    stall_timer_l_ = 0;
    if (state_ == SystemState::FAULT && !hasFault()) {
        forceState(SystemState::READY);
    }
}

bool SafetyMonitor::hasFault() const {
    return faults_.command_timeout || faults_.encoder_right ||
           faults_.encoder_left || faults_.battery_low ||
           faults_.battery_critical || faults_.imu_fault || 
           faults_.motor_stall_r || faults_.motor_stall_l ||
           faults_.lift_fault;
}

void SafetyMonitor::setEncoderFault(bool is_right, bool fault) {
    if (is_right) faults_.encoder_right = fault;
    else faults_.encoder_left = fault;
}

void SafetyMonitor::setIMUFault(bool fault) { faults_.imu_fault = fault; }
void SafetyMonitor::setLiftFault(bool fault) { faults_.lift_fault = fault; }

void SafetyMonitor::resetWatchdog() {
#if ENABLE_WATCHDOG
    wdt_reset();
#endif
}

void SafetyMonitor::forceState(SystemState new_state) {
    state_ = new_state;
}

const char* SafetyMonitor::getStateString() const {
    switch (state_) {
    case SystemState::BOOT:        return "BOOT";
    case SystemState::INIT:        return "INIT";
    case SystemState::CALIBRATION: return "CALIBRATION";
    case SystemState::READY:       return "READY";
    case SystemState::RUNNING:     return "RUNNING";
    case SystemState::PAUSED:      return "PAUSED";
    case SystemState::ESTOP:       return "ESTOP";
    case SystemState::FAULT:       return "FAULT";
    case SystemState::SHUTDOWN:    return "SHUTDOWN";
    default:                       return "UNKNOWN";
    }
}