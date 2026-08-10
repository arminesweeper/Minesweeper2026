/**
 * ============================================================================
 * SYSTEMMANAGER.H - Main Orchestrator & Task Scheduler
 * ============================================================================
 */

#ifndef SYSTEMMANAGER_H
#define SYSTEMMANAGER_H

#include <Arduino.h>
#include "Config.h"
#include "Safety.h"
#include "SerialProtocol.h"
#include "MotionController.h"
#include "Diagnostics.h"
#include "EEPROMManager.h"
#include "MotorDriver.h"
#include "Encoder.h"
#include "PIDController.h"
#include "IMU.h"
#include "Odometry.h"
#include "LiftController.h"
#include "Sensors.h"

struct Task {
    const char* name;
    uint16_t period_ms;
    uint32_t last_run_ms;
    uint32_t max_execution_time_us;
    void (*callback)();
};

class SystemManager {
public:
    static SystemManager& getInstance() {
        static SystemManager instance;
        return instance;
    }

    void begin();
    void update();

private:
    SystemManager() = default;

    static void taskControlLoop();
    static void taskIMU();
    static void taskSensors();
    static void taskLift();
    static void taskHighRateTelemetry();
    static void taskLowRateTelemetry();
    static void taskDiagnostics();

    void handleStateTransitions();
    void processExtendedCommands();
    
    Task tasks_[7];
    
    // Components
    static SafetyMonitor safety_;
    static SerialProtocol serial_;
    static MotionController motion_;
    static Diagnostics diag_;
    static Odometry odom_;
    static IMU imu_;
    static LiftController lift_;
    static Sensors sensors_;
    
    static MotorDriver motorRight_;
    static MotorDriver motorLeft_;
    static MotorDriver motorRight2_;
    static MotorDriver motorLeft2_;
    static Encoder encoderRight_;
    static Encoder encoderLeft_;
    static PIDController pidRight_;
    static PIDController pidLeft_;
};

#endif // SYSTEMMANAGER_H
