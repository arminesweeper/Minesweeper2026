# Arduino Mega 2560 Pin Assignment (As-Fabricated PCB)

**No PCB copper changes.** Firmware VERSION 3 matches shield nets as built.

> External jumpers / flying leads are OK. Cutting or re-spinning the PCB is out of scope.

---

## Synchronized Pin Map

| Pin | Net / Signal | Direction | Function | Connection |
| ----- | -------------- | ----------- | ---------- | ------------ |
| **D18** | B2 | Input | Right encoder B | Primary Harness |
| **D19** | A2 | Input | Right encoder A (INT2) | Primary Harness |
| **D20** | B1 | Input | Left encoder B | Primary Harness |
| **D21** | A1 | Input | Left encoder A (INT0) | Primary Harness |
| **D4** | SERVO | Output | Camera Pan Servo (50 Hz PWM) | Discrete Lead (Preserved) |
| **D7** | LIFT_PWM | Output | Lift actuator PWM (Timer 4 OC4B) | Lift Driver |
| **D49** | LIFT_DIR | Output | Lift actuator DIR | Lift Driver |
| **D53** | LIMIT_TOP | Input | Upper travel limit switch (Active HIGH) | Limit switch |
| **D51** | LIMIT_BOT | Input | Lower travel limit switch (Active HIGH) | Limit switch |
| **D43** | US_TRIG | Output | HC-SR04 Ultrasonic Trigger | Ultrasonic harness |
| **D45** | US_ECHO | Input | HC-SR04 Ultrasonic Echo | Ultrasonic harness |
| **D23** | PROX1 | Input | Proximity Sensor 1 | Sensor harness |
| **D25** | PROX2 | Input | Proximity Sensor 2 | Sensor harness |
| **D27** | PROX3 | Input | Proximity Sensor 3 | Sensor harness |
| **D29** | PROX4 | Input | Proximity Sensor 4 | Sensor harness |
| **D31** | PROX5 | Input | Proximity Sensor 5 | Sensor harness |
| **D33** | MD_PIN | Input | Metal detector (Active HIGH) | Metal detector harness |
| **D37** | SIREN | Output | 12V Siren Relay (Active HIGH) | Siren Relay Module |
| **D38** | MAGNET | Output | Electromagnet Relay (Active HIGH) | Magnet Relay Module |
| **D40** | DIR2 | Output | Left drive motor DIR | Cytron MDD10A |
| **D42** | DIR1 | Output | Right drive motor DIR | Cytron MDD10A |
| **D44** | PWM1 | Output | Right drive motor PWM (Timer 5 OC5C) | Cytron MDD10A |
| **D46** | PWM2 | Output | Left drive motor PWM (Timer 5 OC5A) | Cytron MDD10A |
| **D13** | WARNING_LED | Output | System status LED | Mega onboard LED_BUILTIN |
| **A0** | BAT_SENSE | Input | Battery voltage divider | Main battery |

USB Serial (D0/D1) reserved for Pi ↔ Arduino.
Serial2 (D16/D17) optional for ESP32/WiFi Controller (toggle via ENABLE_SERIAL2 in Config.h).

---

## Encoder / I2C Conflict — Solved Without PCB Edits

As fabricated, ENC1 uses Mega **D20/D21**, which are also the hardware `Wire` pins.

**Solution (firmware + harness):**

1. Keep encoders on D18–D21 exactly as the shield routes them.
2. Enable `IMU_USE_SOFT_I2C` (bit-bang on **D31/D33**).
3. Wire the MPU6050 with **flying leads** to D31 (SDA), D33 (SCL), 5V, GND.
4. **Do not** connect MPU SDA/SCL to shield connectors J29/J38 (those pads share D20/D21 with ENC1 via the Mega).

```text
Shield copper (unchanged):
  D21 ← A1 (left enc)     D20 ← B1 (left enc)
  D19 ← A2 (right enc)    D18 ← B2 (right enc)

Flying lead (not via J29 data):
  MPU SDA → Mega D31
  MPU SCL → Mega D33
```

---

## Timer / Interrupt Allocation

| Resource | Pins | Usage |
| ---------- | ------ | ------- |
| Timer 0 | — | `millis()` — do not reconfigure |
| Timer 3 | D4 | Camera Servo (dedicated 50 Hz PWM via Timer 3, avoids Servo.h Timer 5 hijacking) |
| Timer 5 | D44, D46 | Drive PWMs (hardware PWM, undisturbed) |
| INT2 / INT4 | D21 / D19 | Left ENC A / Right ENC A |
| Soft I2C | D31 / D33 | MPU6050 |

---

## Cytron MDD10A (J36)

| J36 | Signal | Mega |
| ----- | -------- | ------ |
| 1 | DIR1 | D42 |
| 2 | PWM1 | D44 (Timer 5 OC5C) |
| 3 | DIR2 | D40 |
| 4 | PWM2 | D46 (Timer 5 OC5A) |
| 5 | GND | GND |

---

## Magnet / Limits / Siren / Ultrasonic / Gripper

- Magnets: shared relay on **D38** (`MAGNET_SHARED_RELAY`)
- Limits: active HIGH (`LIMIT_SWITCH_ACTIVE_HIGH 1`) on **D51** (top) and **D53** (bottom)
- Ultrasonic Sensor: **D43** (Trig) and **D45** (Echo) for mine detection & distance
- Proximity Array: 5x digital inputs on **D23, D25, D27, D29, D31**
- Metal Detector: digital on **D33** (`METAL_ACTIVE_STATE HIGH`)
- Siren: 12V automotive relay on **D37** (`BUZZER_IS_SIREN_RELAY 1`, `SIREN_ACTIVE_HIGH 1` Active HIGH)
- Camera Pan Servo: PWM on **D4** (Timer 3, preserved per user request)

Full harness details: [WiringManual.md](WiringManual.md).
