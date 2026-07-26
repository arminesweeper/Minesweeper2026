/**
 * ============================================================================
 * SYSTEMMANAGER.CPP - Main Orchestrator & Task Scheduler
 * ============================================================================
 */

#include "SystemManager.h"

// Define static members
SafetyMonitor SystemManager::safety_;
SerialProtocol SystemManager::serial_;
MotionController SystemManager::motion_;
Diagnostics SystemManager::diag_;
Odometry SystemManager::odom_;
IMU SystemManager::imu_;
LiftController SystemManager::lift_;
Sensors SystemManager::sensors_;

MotorDriver SystemManager::motorRight_({Pins::MOTOR_R_PWM, Pins::MOTOR_R_DIR}, false);
MotorDriver SystemManager::motorLeft_({Pins::MOTOR_L_PWM, Pins::MOTOR_L_DIR}, true);
MotorDriver SystemManager::motorGripper_({Pins::MOTOR_LIFT_PWM, Pins::MOTOR_LIFT_DIR}, false); // Example reuse

Encoder SystemManager::encoderRight_({Pins::ENCODER_R_A, Pins::ENCODER_R_B, 5}, false);
Encoder SystemManager::encoderLeft_({Pins::ENCODER_L_A, Pins::ENCODER_L_B, 4}, true);

PIDController SystemManager::pidRight_(PIDTuning::KP_RIGHT, PIDTuning::KI_RIGHT, PIDTuning::KD_RIGHT, 0.0f, PIDTuning::OUTPUT_MIN, PIDTuning::OUTPUT_MAX, Timing::CONTROL_INTERVAL_MS);
PIDController SystemManager::pidLeft_(PIDTuning::KP_LEFT, PIDTuning::KI_LEFT, PIDTuning::KD_LEFT, 0.0f, PIDTuning::OUTPUT_MIN, PIDTuning::OUTPUT_MAX, Timing::CONTROL_INTERVAL_MS);
PIDController SystemManager::pidGripper_(PIDTuning::KP_RIGHT, PIDTuning::KI_RIGHT, PIDTuning::KD_RIGHT, 0.0f, PIDTuning::OUTPUT_MIN, PIDTuning::OUTPUT_MAX, Timing::CONTROL_INTERVAL_MS); // Gripper

// ISR wrappers
void rightEncoderISR() { SystemManager::encoderRight_.handleInterrupt(); }
void leftEncoderISR() { SystemManager::encoderLeft_.handleInterrupt(); }

void SystemManager::begin() {
    EEPROMManager::begin();
    
    Encoder::instance_right_ = &encoderRight_;
    Encoder::instance_left_ = &encoderLeft_;

    motorRight_.begin();
    motorLeft_.begin();
    motorGripper_.begin();

    encoderRight_.begin(rightEncoderISR);
    encoderLeft_.begin(leftEncoderISR);

    pidRight_.begin();
    pidLeft_.begin();
    pidGripper_.begin();

    serial_.begin();
    motion_.begin();
    diag_.begin();
    odom_.begin();
    
#if ENABLE_IMU
    const RobotParameters& params = EEPROMManager::getParams();
    imu_.begin(params.gyro_bias_x, params.gyro_bias_y, params.gyro_bias_z);
#endif
#if ENABLE_SENSORS
    sensors_.begin();
#endif
#if ENABLE_LIFT
    lift_.begin();
#endif

    safety_.begin();

    // Initialize tasks
    tasks_[0] = {"Control", Timing::CONTROL_INTERVAL_MS, 0, 0, taskControlLoop};
    tasks_[1] = {"IMU", Timing::IMU_INTERVAL_MS, 0, 0, taskIMU};
    tasks_[2] = {"Sensors", Timing::SENSOR_INTERVAL_MS, 0, 0, taskSensors};
    tasks_[3] = {"Lift", Timing::LIFT_INTERVAL_MS, 0, 0, taskLift};
    tasks_[4] = {"Hi_Telem", 50, 0, 0, taskHighRateTelemetry}; // 20 Hz
    tasks_[5] = {"Lo_Telem", 500, 0, 0, taskLowRateTelemetry}; // 2 Hz
    tasks_[6] = {"Diag", 1000, 0, 0, taskDiagnostics}; // 1 Hz
}

void SystemManager::update() {
    diag_.loopStart();
    safety_.resetWatchdog();

    // Process serial input (Non-blocking)
    float r_vel = 0.0f, l_vel = 0.0f, g_vel = 0.0f;
    if (serial_.processInput(r_vel, l_vel, g_vel)) {
        motion_.setRightTarget(r_vel);
        motion_.setLeftTarget(l_vel);
        // Gripper velocity handling here
        diag_.incrementCommandCount();
        if (safety_.getState() == SystemState::ESTOP) {
            safety_.clearEStop();
        }
        sensors_.beep();
    }
    processExtendedCommands();

    uint32_t now = millis();
    
    // Cooperative Scheduler execution
    for (int i = 0; i < 7; i++) {
        if (now - tasks_[i].last_run_ms >= tasks_[i].period_ms) {
            tasks_[i].last_run_ms = now;
            
            uint32_t t_start = micros();
            tasks_[i].callback();
            uint32_t dt = micros() - t_start;
            
            if (dt > tasks_[i].max_execution_time_us) {
                tasks_[i].max_execution_time_us = dt;
            }
        }
    }
    
    // Update Safety Machine
    SystemState next_state = safety_.update(serial_.getLastCommandTime(),
                                            motion_.getRightState().measured_velocity,
                                            motion_.getLeftState().measured_velocity,
                                            motorRight_.getOutput(),
                                            motorLeft_.getOutput());
    
    handleStateTransitions();
    diag_.loopEnd();
}

void SystemManager::taskControlLoop() {
    float dt = Timing::CONTROL_INTERVAL_MS / 1000.0f;
    
    float right_meas_vel = encoderRight_.calculateVelocity(encoderRight_.getPulsesAndReset(), dt);
    float left_meas_vel = encoderLeft_.calculateVelocity(encoderLeft_.getPulsesAndReset(), dt);
    
    motion_.getRightStateMutable().measured_velocity = right_meas_vel;
    motion_.getLeftStateMutable().measured_velocity = left_meas_vel;
    
    motion_.updateProfiles(dt);
    
    motion_.getRightStateMutable().target_pwm = pidRight_.compute(motion_.getRightState().profiled_velocity, right_meas_vel);
    motion_.getLeftStateMutable().target_pwm = pidLeft_.compute(motion_.getLeftState().profiled_velocity, left_meas_vel);
    
    motion_.updatePWM(dt);
    
    if (safety_.isRunning() || safety_.getState() == SystemState::READY) {
        motorRight_.setOutput(motion_.getRightState().current_pwm);
        motorLeft_.setOutput(motion_.getLeftState().current_pwm);
    } else {
        motorRight_.emergencyStop();
        motorLeft_.emergencyStop();
    }
    
    odom_.update(right_meas_vel, left_meas_vel, dt);
    diag_.incrementControlCycleCount();
}

void SystemManager::taskIMU() {
#if ENABLE_IMU
    float dt = Timing::IMU_INTERVAL_MS / 1000.0f;
    imu_.update(dt);
    safety_.setIMUFault(imu_.hasError());
#endif
}

void SystemManager::taskSensors() {
#if ENABLE_SENSORS
    static bool last_metal = false;
    sensors_.update();
    bool current_metal = sensors_.isMetalDetected();
    if (current_metal && !last_metal) {
        serial_.sendMetalDetect(true);
        sensors_.setBuzzerPattern(BuzzerPattern::MINE_DETECT);
    } else if (!current_metal && last_metal) {
        serial_.sendMetalDetect(false);
        sensors_.setBuzzerPattern(BuzzerPattern::SILENT);
    }
    last_metal = current_metal;
#endif
}

void SystemManager::taskLift() {
#if ENABLE_LIFT
    LiftState prev_state = lift_.getState();
    lift_.update();
    safety_.setLiftFault(lift_.hasFault());
    if (lift_.getState() != prev_state) {
        serial_.sendLiftState(lift_.getStateString(), lift_.getMagnetState());
    }
#endif
}

void SystemManager::taskHighRateTelemetry() {
    serial_.sendTelemetry(motion_.getRightState().measured_velocity,
                          motion_.getLeftState().measured_velocity,
                          0.0f); // Gripper vel
}

void SystemManager::taskLowRateTelemetry() {
#if ENABLE_ODOMETRY
    serial_.sendOdometry(odom_.getX(), odom_.getY(), odom_.getTheta());
#endif
#if ENABLE_IMU
    const IMUData& id = imu_.getData();
    serial_.sendIMU(id.yaw, id.pitch, id.roll, id.accel_x, id.accel_y, id.accel_z);
#endif
#if ENABLE_SENSORS
    uint16_t prox[5];
    sensors_.getAllProximity(prox);
    serial_.sendProximity(prox, 5);
#endif
}

void SystemManager::taskDiagnostics() {
    // Expose stats logic here
    serial_.sendDiagnostics(diag_.getLoopFrequency(), diag_.getCPUUsagePercent(), safety_.hasFault() ? 1 : 0);
}

void SystemManager::handleStateTransitions() {
    switch (safety_.getState()) {
        case SystemState::READY:
        case SystemState::RUNNING:
            sensors_.setLEDPattern(LEDPattern::OFF);
            break;
        case SystemState::ESTOP:
            motorRight_.emergencyStop();
            motorLeft_.emergencyStop();
            motion_.emergencyStop();
            sensors_.setLEDPattern(LEDPattern::SLOW_BLINK);
            break;
        case SystemState::FAULT:
        case SystemState::SHUTDOWN:
            motorRight_.emergencyStop();
            motorLeft_.emergencyStop();
            motion_.emergencyStop();
            sensors_.setLEDPattern(LEDPattern::FAST_BLINK);
            break;
        default:
            motorRight_.emergencyStop();
            motorLeft_.emergencyStop();
            break;
    }
}

void SystemManager::processExtendedCommands() {
    char cmd[SerialConfig::CMD_BUFFER_SIZE];
    if (serial_.getExtendedCommand(cmd, sizeof(cmd))) {
        if (strcmp(cmd, "CESTOP") == 0) safety_.triggerEStop();
        else if (strcmp(cmd, "CCLEAR") == 0) safety_.clearFaults();
        else if (strcmp(cmd, "CRESET") == 0) {
            odom_.reset();
            imu_.resetYaw();
            diag_.resetStats();
            serial_.sendStatus("Reset complete");
        }
    }
}
