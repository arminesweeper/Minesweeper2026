/**
 * ============================================================================
 * MOTORCONTROLLER.INO - Main Application Entry Point
 * ============================================================================
 * Minesweeper Robot Motor Controller
 *
 * Hardware:
 *   - Arduino Mega 2560 (ATmega2560-16U2)
 *   - Cytron MDD10A Rev 2.0 Motor Driver
 *   - Dual Quadrature Encoders
 *   - MPU6050 IMU
 *   - Lift Mechanism with shared electromagnet relay (PCB Rev A)
 *   - 5 Analog Proximity Sensors, Digital Metal Detector
 *   - Siren relay / warning LED
 *
 * Pin map: see Config.h and Docs/Pinout.md (synchronized to Robotics_Mine PCB).
 * Wiring:  see Docs/WiringManual.md
 * Architecture:
 *   - Modular multi-file design
 *   - Non-blocking control loop using Cooperative Scheduler
 *   - Hardware abstraction layers
 *   - Safety monitoring with watchdog
 * ============================================================================
 */

#include "Config.h"

#if !TEST_MODE
#include "SystemManager.h"

void setup() { SystemManager::getInstance().begin(); }

void loop() { SystemManager::getInstance().update(); }

#else
// ============================================================================
// TEST MODE - Direct Serial Control (Bypass SystemManager)
// ============================================================================
#include "MotorDriver.h"

MotorDriver rightMotor({Pins::MOTOR_R_PWM, Pins::MOTOR_R_DIR}, false);
MotorDriver leftMotor({Pins::MOTOR_L_PWM, Pins::MOTOR_L_DIR}, true);
MotorDriver rightMotor2({Pins::MOTOR_R_PWM_2, Pins::MOTOR_R_DIR_2}, false);
MotorDriver leftMotor2({Pins::MOTOR_L_PWM_2, Pins::MOTOR_L_DIR_2}, true);

void setup() {
  Serial.begin(115200);
  Serial2.begin(115200);
  rightMotor.begin();
  leftMotor.begin();
  rightMotor2.begin();
  leftMotor2.begin();
  Serial.println(F("========================================"));
  Serial.println(F(" MOTOR TEST MODE ENABLED (Config.h)"));
  Serial.println(F("========================================"));
  Serial.println(F("Send commands via Serial Monitor:"));
  Serial.println(F(" 'w' - Forward"));
  Serial.println(F(" 's' - Backward"));
  Serial.println(F(" 'a' - Left"));
  Serial.println(F(" 'd' - Right"));
  Serial.println(F(" 'x' - Stop"));
  Serial.println(F("========================================"));
}

void loop() {
  Stream* activeStream = nullptr;
  if (Serial.available() > 0) activeStream = &Serial;
  else if (Serial2.available() > 0) activeStream = &Serial2;

  if (activeStream != nullptr) {
    char c = activeStream->read();

    // Ignore newlines and carriage returns
    if (c == '\n' || c == '\r')
      return;

    float speed = 100.0f; // Default test PWM speed

    switch (c) {
    case 'w':
    case 'W':
      rightMotor.setOutput(speed);
      leftMotor.setOutput(speed);
      rightMotor2.setOutput(speed);
      leftMotor2.setOutput(speed);
      activeStream->println(F("-> Forward"));
      break;
    case 's':
    case 'S':
      rightMotor.setOutput(-speed);
      leftMotor.setOutput(-speed);
      rightMotor2.setOutput(-speed);
      leftMotor2.setOutput(-speed);
      activeStream->println(F("-> Backward"));
      break;
    case 'a':
    case 'A':
      rightMotor.setOutput(speed);
      leftMotor.setOutput(-speed);
      rightMotor2.setOutput(speed);
      leftMotor2.setOutput(-speed);
      activeStream->println(F("-> Left"));
      break;
    case 'd':
    case 'D':
      rightMotor.setOutput(-speed);
      leftMotor.setOutput(speed);
      rightMotor2.setOutput(-speed);
      leftMotor2.setOutput(speed);
      activeStream->println(F("-> Right"));
      break;
    case 'x':
    case 'X':
    case ' ':
      rightMotor.setOutput(0);
      leftMotor.setOutput(0);
      rightMotor2.setOutput(0);
      leftMotor2.setOutput(0);
      activeStream->println(F("-> Stop"));
      break;
    default:
      activeStream->println(F("Unknown command. Use w, a, s, d, x."));
      break;
    }
  }
}
#endif