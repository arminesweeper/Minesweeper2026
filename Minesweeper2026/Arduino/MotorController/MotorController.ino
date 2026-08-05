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

#include "SystemManager.h"

void setup() {
  SystemManager::getInstance().begin();
}

void loop() {
  SystemManager::getInstance().update();
}