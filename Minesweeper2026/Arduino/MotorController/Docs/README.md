# Minesweeper Robot — Motor Controller Firmware

**Competition-grade embedded firmware for the Assiut Robotics Team autonomous minesweeper robot.**

| Field | Value |
|-------|-------|
| Target Board | Arduino Mega 2560 (ATmega2560-16U2) |
| Framework | Arduino / AVR C++ |
| Motor Driver | Cytron MDD10A Rev 2.0 |
| Drive | 4WD Differential Drive |
| High-Level Host | Raspberry Pi 4 (ROS 2) |
| Communication | USB Serial @ 115 200 baud |
| Firmware Version | 2.0.0 |

---

## Table of Contents

1. [Project Overview](#project-overview)
2. [Architecture](#architecture)
3. [Folder Structure](#folder-structure)
4. [Dependencies](#dependencies)
5. [Build Instructions](#build-instructions)
6. [Pinout Summary](#pinout-summary)
7. [Communication Protocol](#communication-protocol)
8. [Safety Systems](#safety-systems)
9. [Testing](#testing)
10. [Future Improvements](#future-improvements)

---

## Project Overview

This firmware runs on the Arduino Mega 2560 and controls all low-level hardware for the minesweeper robot.  The Raspberry Pi 4 handles high-level autonomy (navigation, vision, path planning) via ROS 2 and sends velocity commands to the Arduino over USB serial.  The Arduino never receives direct motor PWM values — it always closes the velocity loop locally.

### Capabilities

| Subsystem | Description |
|-----------|-------------|
| Motor Control | Closed-loop PID velocity control for left/right drive motors |
| Motion Planning | Velocity ramping, deadband compensation, acceleration limiting |
| Odometry | Differential-drive pose estimation (x, y, θ) |
| IMU | MPU6050 yaw/pitch/roll via complementary filter |
| Lift Mechanism | DC motor with limit switches and state machine |
| Electromagnets | 5 individually addressable electromagnets for mine pickup |
| Metal Detector | Digital mine detection input with debounce |
| Proximity Sensors | 5 analog proximity sensors with configurable thresholds |
| Safety | Watchdog, command timeout, battery monitoring, E-Stop |
| Diagnostics | Loop timing, command stats, SRAM usage, uptime |

---

## Architecture

The firmware follows a layered architecture.  See [Architecture.md](Architecture.md) for the full description.

```
┌─────────────────────────────────────────────┐
│                  ROS 2 (Pi)                 │
└──────────────────────┬──────────────────────┘
                       │ USB Serial
┌──────────────────────▼──────────────────────┐
│              SerialProtocol                 │
│         (Parser / Telemetry / Errors)       │
├─────────────────────────────────────────────┤
│          MotionController / LiftController  │
│       (Ramping / Deadband / State Machine)  │
├─────────────────────────────────────────────┤
│     PIDController     │      Odometry       │
│  (Anti-Windup / Sat)  │   (Diff-Drive Kin)  │
├───────────────────────┼─────────────────────┤
│  MotorDriver │ Encoder │  IMU  │  Sensors   │
│  (Cytron HAL)│(Quad ISR)│(MPU) │(Prox/Metal)│
├─────────────────────────────────────────────┤
│       Safety  │  Diagnostics  │  Config     │
│  (WDT/E-Stop) │ (Stats/SRAM)  │ (Pins/Tune)│
└─────────────────────────────────────────────┘
```

---

## Folder Structure

```
MotorController/
├── MotorController.ino     Main sketch — setup() and loop()
├── Config.h                Pin definitions, tuning, feature flags
├── MotorDriver.h/.cpp      Cytron MDD10A motor HAL
├── Encoder.h/.cpp          Quadrature encoder with ISR
├── PIDController.h/.cpp    Custom PID with anti-windup
├── MotionController.h/.cpp Velocity ramping and deadband
├── SerialProtocol.h/.cpp   ROS serial command parser + telemetry
├── Safety.h/.cpp           Watchdog, timeout, fault detection
├── Diagnostics.h/.cpp      Loop timing, stats, SRAM reporting
├── Odometry.h/.cpp         Differential drive pose tracking
├── IMU.h/.cpp              MPU6050 driver + complementary filter
├── LiftController.h/.cpp   Lift state machine + electromagnets
├── Sensors.h/.cpp          Proximity, metal detector, buzzer, LED
├── README.md               This file
├── Architecture.md         Software architecture documentation
├── API.md                  Full API reference (Doxygen-style)
├── Pinout.md               Arduino Mega pin assignment table
├── Communication.md        Serial protocol specification
├── Safety.md               Safety system documentation
├── MotionControl.md        Motion control theory and tuning
└── Testing.md              Test plan and procedures
```

---

## Dependencies

| Library | Version | Purpose | Notes |
|---------|---------|---------|-------|
| Arduino Core | 1.8+ | AVR framework | Built-in |
| Wire | Built-in | I2C for MPU6050 | Arduino standard library |

**No external libraries are required.**  The firmware uses a fully custom PID implementation and direct MPU6050 register access via the Wire library.  There is no dependency on PID_v1, Adafruit, or any third-party library.

---

## Build Instructions

### Using Arduino IDE

1. Open `MotorController/MotorController.ino` in Arduino IDE.
2. Select **Board → Arduino Mega or Mega 2560**.
3. Select the correct **Port** (USB connection to Mega).
4. Click **Verify** (✓) to compile.
5. Click **Upload** (→) to flash.

### Using Arduino CLI

```bash
# Compile
arduino-cli compile --fqbn arduino:avr:mega MotorController/

# Upload
arduino-cli upload --fqbn arduino:avr:mega --port /dev/ttyUSB0 MotorController/
```

### Using PlatformIO

Create a `platformio.ini` in the project root:

```ini
[env:mega]
platform = atmelavr
board = megaatmega2560
framework = arduino
monitor_speed = 115200
```

```bash
pio run            # Compile
pio run -t upload  # Upload
```

---

## Pinout Summary

See [Pinout.md](Pinout.md) for the complete pin assignment table.

| Function | Pin(s) | Type |
|----------|--------|------|
| Right Motor PWM | D9 | PWM Output |
| Right Motor DIR | D12 | Digital Output |
| Left Motor PWM | D11 | PWM Output |
| Left Motor DIR | D7 | Digital Output |
| Right Encoder A | D3 (INT5) | External Interrupt |
| Right Encoder B | D5 | Digital Input |
| Left Encoder A | D2 (INT4) | External Interrupt |
| Left Encoder B | D4 | Digital Input |
| MPU6050 SDA | D20 | I2C |
| MPU6050 SCL | D21 | I2C |
| Lift Motor PWM | D10 | PWM Output |
| Lift Motor DIR | D8 | Digital Output |
| Electromagnets 1-5 | D22-D26 | Digital Output |
| Limit Switch Top | D28 | Digital Input (Pull-up) |
| Limit Switch Bottom | D29 | Digital Input (Pull-up) |
| Metal Detector | D27 | Digital Input (Pull-up) |
| Proximity 1-5 | A1-A5 | Analog Input |
| Battery Voltage | A0 | Analog Input |
| Buzzer | D6 | PWM Output |
| Warning LED | D13 | Digital Output |

---

## Communication Protocol

See [Communication.md](Communication.md) for the full specification.

### Command Format (ROS → Arduino)

```
rp02.50,ln01.30,
```

| Field | Meaning |
|-------|---------|
| `r` | Right wheel command |
| `l` | Left wheel command |
| `p` | Positive (forward) direction |
| `n` | Negative (reverse) direction |
| `XX.XX` | Velocity magnitude in rad/s |
| `,` | Field separator |

### Extended Commands (Auxiliary Hardware)

Commands prefixed with `C` control auxiliary hardware subsystems. Must be terminated by a newline (`\n`).

| Command | Action |
|---------|--------|
| `CLIFT:UP\n` | Raise the lift mechanism |
| `CLIFT:DN\n` | Lower the lift mechanism |
| `CLIFT:STOP\n` | Stop the lift motor |
| `CMAG:<1-5>:ON\n` | Energize specific electromagnet (1-5) |
| `CMAG:ALL:ON\n` | Energize all electromagnets |
| `CMAG:ALL:OFF\n` | De-energize all electromagnets |
| `CBUZZ:BEEP\n` | Trigger a short buzzer beep |
| `CBUZZ:ALERT\n` | Trigger a repeating alert pattern |

### Telemetry Format (Arduino → ROS)

```
rp2.310,ln1.280,
```

Same format as command but with measured velocities.

### Extended Telemetry (new channels)

```
O:0.1234,0.5678,1.2345,       Odometry (x, y, θ)
M:1                            Metal detector (0/1)
P:120,340,560,780,900          Proximity sensors (5 values)
I:45.2,-2.1,0.3                IMU (yaw, pitch, roll degrees)
L:RAISED                       Lift state
D:STATE=1,LOOPS=...            Diagnostics
S:message                      Status
E:code:message                 Error
```

---

## Safety Systems

See [Safety.md](Safety.md) for complete documentation.

| Feature | Description | Default |
|---------|-------------|---------|
| Hardware Watchdog | 2-second WDT timeout | Enabled |
| Command Timeout | Motors stop after 1500 ms without commands | Enabled |
| Battery Monitor | Low/high voltage detection via ADC | Enabled |
| Velocity Sanity | Rejects commands > 10 rad/s | Always |
| Encoder Fault | Detects disconnected encoders | Enabled |
| Lift Fault | Stops motors if lift stalls or limit switches fail | Enabled |
| IMU Disconnect | Stops motors if MPU6050 I2C communication drops | Enabled |
| E-Stop | Manual emergency stop via serial command | Available |
| Safe Startup | Motors held off until first valid command | Always |

---

## Testing

See [Testing.md](Testing.md) for the full test plan.

### Quick Smoke Test

1. Flash the firmware.
2. Open serial monitor at 115200 baud.
3. Expect to see: `MC:READY` and `S:Minesweeper Motor Controller v2.0`.
4. Send `rp01.00,lp01.00,` — both wheels should spin forward at ~1 rad/s.
5. Send `rp00.00,lp00.00,` — both wheels stop.
6. Wait 1.5 seconds without sending — motors auto-stop (timeout).
7. Verify telemetry lines appear at 10 Hz.

---

## Future Improvements

| Priority | Feature | Description |
|----------|---------|-------------|
| High | Checksum | Add CRC-8 to serial packets for error detection |
| High | IMU Heading PID | Close heading loop using MPU6050 yaw |
| Medium | EEPROM Config | Store PID gains and calibration in EEPROM |
| Medium | Motor Current Sense | Add current sensing for stall detection |
| Medium | Wheel Slip Detection | Compare IMU yaw rate vs differential-drive estimate |
| Low | DMA Serial | Use USART DMA for zero-copy serial I/O |
| Low | FreeRTOS | Migrate to RTOS for deterministic task scheduling |
| Low | CAN Bus | Replace serial with CAN for multi-board communication |
