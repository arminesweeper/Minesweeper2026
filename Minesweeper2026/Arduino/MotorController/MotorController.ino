/**
 * ============================================================================
 * MOTORCONTROLLER.INO - Main Application Entry Point
 * ============================================================================
 * Minesweeper Robot Motor Controller
 *
 * Hardware:
 *   - Arduino Mega 2560 (ATmega2560-16U2)
 *   - Cytron MDD10A Rev 2.0 Motor Driver (PWM 44/46, DIR 42/40)
 *   - Dual Quadrature Encoders (Right: D19/D18, Left: D21/D20)
 *   - Lift Mechanism: PWM D7 (Timer 4 OC4B), DIR D49, Limits D51/D53 (Active HIGH)
 *   - HC-SR04 Ultrasonic Distance Sensor: Trig D43, Echo D45
 *   - 5 Digital Proximity Sensors: D23, D25, D27, D29, D31
 *   - Digital Metal Detector: D33 (Active HIGH)
 *   - 12V Automotive Siren Relay: D37
 *   - Shared Electromagnet Relay: D38
 *   - Camera Pan Servo: D4 (Timer 3 custom 50 Hz PWM)
 *
 * Pin map: see Config.h and Docs/Pinout.md (synchronized to Safi robot wiring).
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
#if ENABLE_SERIAL2
  Serial2.begin(115200);
#endif
  rightMotor.begin();
  leftMotor.begin();
  rightMotor2.begin();
  leftMotor2.begin();
  Serial.println(F("========================================"));
  Serial.println(F("   MOTOR TEST MODE ENABLED (Config.h)   "));
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
#if ENABLE_SERIAL2
  else if (Serial2.available() > 0) activeStream = &Serial2;
#endif

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