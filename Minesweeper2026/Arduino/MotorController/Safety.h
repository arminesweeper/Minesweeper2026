/**
 * ============================================================================
 * SAFETY.H - Safety Systems & Fault Detection
 * ============================================================================
 */

#ifndef SAFETY_H
#define SAFETY_H

#include "Config.h"
#include <Arduino.h>

/**
 * @brief System operating state enumeration.
 */
enum class SystemState : uint8_t {
    BOOT,
    INIT,
    CALIBRATION,
    READY,
    RUNNING,
    PAUSED,
    ESTOP,
    FAULT,
    SHUTDOWN
};

struct FaultFlags {
    bool command_timeout : 1;
    bool encoder_right   : 1;
    bool encoder_left    : 1;
    bool motor_stall_r   : 1;
    bool motor_stall_l   : 1;
    bool battery_low     : 1;
    bool battery_critical: 1;
    bool imu_fault       : 1;
    bool lift_fault      : 1;
    bool watchdog_reset  : 1;
};

class SafetyMonitor {
public:
    SafetyMonitor();

    void begin();

    /**
     * @brief Run all safety checks.
     */
    SystemState update(unsigned long last_command_time, 
                       float right_vel, float left_vel,
                       float right_pwm, float left_pwm);

    void triggerEStop();
    void clearEStop();
    
    void triggerFault(const char* reason);
    void clearFaults();

    SystemState getState() const { return state_; }
    bool isRunning() const { return state_ == SystemState::RUNNING; }
    FaultFlags getFaults() const { return faults_; }
    bool hasFault() const;

    void setIMUFault(bool fault);
    void setLiftFault(bool fault);
    void setEncoderFault(bool is_right, bool fault);

    float readBatteryVoltage();
    void resetWatchdog();
    void forceState(SystemState new_state);
    
    const char* getStateString() const;

private:
    SystemState state_;
    FaultFlags faults_;
    unsigned long estop_trigger_time_;
    
    // Battery filtering
    float filtered_battery_mv_;
    bool battery_initialized_;

    // Stall timers
    unsigned long stall_timer_r_;
    unsigned long stall_timer_l_;

    void checkBatteryVoltage();
    void checkMotorStall(float right_vel, float left_vel, float right_pwm, float left_pwm);
};

#endif // SAFETY_H