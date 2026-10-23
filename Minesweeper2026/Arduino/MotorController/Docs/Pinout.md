# Arduino Mega 2560 Pin Assignment (Final As-Built Robot Wiring)

**Fully synchronized with `Config.h`, `MotorController.ino`, and physical robot wiring (Safi configuration).**

> [!NOTE]
> All pin definitions match the active codebase in `Minesweeper2026/Arduino/MotorController/Config.h`.
> Primary drive motors and wheel encoders utilize direct shield copper tracks. Unrouted peripherals use dedicated jumper harnesses and flying leads.

---

## 1. Complete Synchronized Pin Map

| Pin | Net / Signal | Direction | Function | Hardware Subsystem & Connection |
| :---: | :---: | :---: | :--- | :--- |
| **D18** | `B2` | Input | Right Wheel Encoder Phase B | Primary Harness / Shield Copper (J17) |
| **D19** | `A2` | Input | Right Wheel Encoder Phase A (INT2) | Primary Harness / Shield Copper (J17) |
| **D20** | `B1` | Input | Left Wheel Encoder Phase B | Primary Harness / Shield Copper (J16) |
| **D21** | `A1` | Input | Left Wheel Encoder Phase A (INT0) | Primary Harness / Shield Copper (J16) |
| **D4** | `SERVO` | Output | Camera Pan Servo (Timer 3, 50 Hz PWM) | Discrete Lead / Signal Header (J13) |
| **D7** | `LIFT_PWM` | Output | Lift Actuator Motor Speed (Timer 4 OC4B) | Lift Motor Driver (J37 / J44) |
| **D49** | `LIFT_DIR` | Output | Lift Actuator Motor Direction | Lift Motor Driver (J37 / J44) |
| **D51** | `LIMIT_TOP` | Input | Upper Travel Limit Switch (Active HIGH) | Top Switch (J43 / J44) |
| **D53** | `LIMIT_BOT` | Input | Lower Travel Limit Switch (Active HIGH) | Bottom Switch (J43 / J44) |
| **D43** | `US_TRIG` | Output | HC-SR04 Ultrasonic Trigger Pulse | Gripper Ultrasonic Harness |
| **D45** | `US_ECHO` | Input | HC-SR04 Ultrasonic Echo Pulse | Gripper Ultrasonic Harness |
| **D23** | `PROX1` | Input | Digital Optical Proximity Sensor 1 | Sensor Harness (APRX1 via J39) |
| **D25** | `PROX2` | Input | Digital Optical Proximity Sensor 2 | Sensor Harness (APRX2 via J39) |
| **D27** | `PROX3` | Input | Digital Optical Proximity Sensor 3 | Sensor Harness (APRX3 via J39) |
| **D29** | `PROX4` | Input | Digital Optical Proximity Sensor 4 | Sensor Harness (APRX4 via J39) |
| **D31** | `PROX5` | Input | Digital Optical Proximity Sensor 5 | Sensor Harness (APRX5 via J39) |
| **D33** | `MD_PIN` | Input | Pulse Induction Metal Detector (Active HIGH) | Search Coil Harness (J40) |
| **D34** | `IMU_SDA` | In/Out | MPU6050 Soft-I2C Serial Data (Flying Lead) | Isolated from Prox/MD (Avoids D31 clash) |
| **D36** | `IMU_SCL` | Output | MPU6050 Soft-I2C Serial Clock (Flying Lead) | Isolated from Prox/MD (Avoids D33 clash) |
| **D37** | `SIREN` | Output | 12V Siren Automotive Relay K2 | Relay K2 Driver (J21 / J18) |
| **D38** | `MAGNET` | Output | Electromagnet Array Relay K1 | Relay K1 Driver (J19 / MAG1–MAG5) |
| **D40** | `DIR2` | Output | Left Drive Motor Direction (DIR2) | Cytron MDD10A (Connector J36) |
| **D42** | `DIR1` | Output | Right Drive Motor Direction (DIR1) | Cytron MDD10A (Connector J36) |
| **D44** | `PWM1` | Output | Right Drive Motor Speed (Timer 5 OC5C) | Cytron MDD10A (Connector J36) |
| **D46** | `PWM2` | Output | Left Drive Motor Speed (Timer 5 OC5A) | Cytron MDD10A (Connector J36) |
| **D13** | `WARN_LED` | Output | Diagnostic & Warning Indicator | Mega Onboard `LED_BUILTIN` |
| **A0** | `BAT_SENSE` | Input | Main Battery Voltage Divider (3.3:1 Ratio) | Scaled Analog Battery Sense |
| **D0 / D1** | `UART0` | In/Out | USB Serial Communication (115200 baud) | Raspberry Pi 4 ↔ Arduino Mega Link |
| **D16 / D17** | `UART2` | In/Out | Hardware Serial2 (115200 baud) | ESP8266 WiFi Web Controller |

---

## 2. Secondary 4WD Expansion Motors (Cytron 2 / Auxiliary)

| Pin | Signal | Direction | Function | Connection |
| :---: | :---: | :---: | :--- | :--- |
| **D9** | `MOTOR_R_PWM_2` | Output | Secondary Right Motor PWM (Timer 2, OC2B) | Cytron 2 Header |
| **D8** | `MOTOR_R_DIR_2` | Output | Secondary Right Motor DIR | Cytron 2 Header |
| **D11** | `MOTOR_L_PWM_2` | Output | Secondary Left Motor PWM (Timer 1, OC1A) | Cytron 2 Header |
| **D10** | `MOTOR_L_DIR_2` | Output | Secondary Left Motor DIR | Cytron 2 Header |

---

## 3. Resolving the Hardware I²C & Encoder Conflict

On the fabricated PCB, Encoder 1 (Left Wheel) tracks were routed to **D20 / D21** to utilize external hardware interrupts (`INT0`). However, D20/D21 are also the Mega's hardware Wire (`SDA` / `SCL`) lines.

### Solution Implemented in Final Code

1. Keep the left encoder on **D20 / D21** (maintains interrupt-driven quadrature reading without PCB copper cutting).
2. Enable `IMU_USE_SOFT_I2C 1` in `Config.h`.
3. Route Soft-I2C to **D34 (SDA)** and **D36 (SCL)** using dedicated flying leads:
   - *Note:* Older drafts used D30/D31 or D31/D33, which caused conflicts with Proximity 5 (D31) and Metal Detector (D33). The final code permanently assigns **D34 and D36** to eliminate pin contention.
4. **Do NOT connect MPU6050 data pins to shield headers J29/J38**, as those headers are tied to D20/D21 copper traces.

```text
PCB Copper (Intact):
  Mega D21 ← ENC1 Phase A (Left Interrupt INT0)
  Mega D20 ← ENC1 Phase B (Left Direction)

Dedicated Flying Leads:
  MPU6050 SDA ────→ Mega Pin D34
  MPU6050 SCL ────→ Mega Pin D36
  MPU6050 VCC ────→ Mega 5V / Sensor Power
  MPU6050 GND ────→ Common GND
```

---

## 4. Timer & Interrupt Resource Allocation

| Hardware Timer | Allocated Pins | Frequency / Mode | Subsystem | Notes |
| :--- | :---: | :---: | :--- | :--- |
| **Timer 0** | — | ~976 Hz | System `millis()` and `micros()` | Do not reconfigure |
| **Timer 3** | **D4** | 50 Hz PWM | Camera Pan Servo | Custom 50 Hz generation; preserves Timer 5 for motors |
| **Timer 4** | **D7** | ~490 Hz PWM (OC4B) | Lift Actuator Motor Speed | Dedicated hardware PWM for mine elevator |
| **Timer 5** | **D44, D46** | ~490 Hz PWM (OC5A/OC5C) | Drive Wheels PWM1 & PWM2 | Undisturbed dual drive motor PWM |
| **INT0 (Pin D21)** | **D21** | Rising/Falling edge | Left Wheel Encoder A | Quadrature decode in ISR |
| **INT2 (Pin D19)** | **D19** | Rising/Falling edge | Right Wheel Encoder A | Quadrature decode in ISR |

---

## 5. Actuator & Sensor Electrical Logic Levels

- **Drive Motors (Cytron MDD10A):** Controlled via `PWM` duty cycle (0–255) and digital `DIR` (HIGH = Forward, LOW = Reverse).
- **Lift Actuator:** Driven via `D7` (PWM speed clamped to 90 / ~35% power in `LiftConfig::LIFT_PWM_SPEED`) and `D49` (DIR).
- **Limit Switches:** Wired to `D51` (Top) and `D53` (Bottom). Active-HIGH electrical logic (`LIMIT_SWITCH_ACTIVE_HIGH 1`). Bypassed when `#define ENABLE_LIMIT_SWITCHES 0` is set for pure manual teleoperation.
- **Electromagnets (K1):** Shared relay on `D38` (`MAGNET_SHARED_RELAY 1`). Energizes all 5 magnets simultaneously with 8-second auto-off timer protection.
- **Siren Relay (K2):** Driven by `D37` via 2N2222A transistor to 12V relay coil. Configured as Active-HIGH in final `Config.h` (`SIREN_ACTIVE_HIGH 1`).
- **Proximity Array:** 5× digital inputs on `D23, D25, D27, D29, D31` with threshold filtering (`PROXIMITY_THRESHOLD 500`).
- **Metal Detector:** Digital pulse induction input on `D33` (Active HIGH).
- **Ultrasonic Sensor (HC-SR04):** `D43` (Trig) and `D45` (Echo) for mine detection inside the gripper scoop.
