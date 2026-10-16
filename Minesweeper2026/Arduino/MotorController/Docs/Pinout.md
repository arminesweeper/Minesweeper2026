# Arduino Mega 2560 Pin Assignment (As-Fabricated PCB)

**No PCB copper changes.** Firmware VERSION 3 matches shield nets as built.

> External jumpers / flying leads are OK. Cutting or re-spinning the PCB is out of scope.

---

## Synchronized Pin Map

| Pin | Net / Signal | Direction | Function | Connection |
|-----|--------------|-----------|----------|------------|
| **D18** | B2 | Input | Right encoder B | PCB copper |
| **D19** | A2 | Input | Right encoder A (INT2) | PCB copper |
| **D20** | B1 | Input | Left encoder B | PCB copper |
| **D21** | A1 | Input | Left encoder A (INT2) | PCB copper |
| **D4**  | SERVO | Output | Camera / gripper servo | Discrete lead / J13 |
| **D8**  | DIR1_2 | Output | Secondary Right motor DIR | Cytron 2 |
| **D9**  | PWM1_2 | Output | Secondary Right motor PWM | Cytron 2 |
| **D10** | DIR2_2 | Output | Secondary Left motor DIR | Cytron 2 |
| **D11** | PWM2_2 | Output | Secondary Left motor PWM | Cytron 2 |
| **D25** | GRIPPER_SENSE | Input (Pullup) | Gripper break-beam / optical sensor | Jumper / sensor harness |
| **D31** | soft SDA | I/O | MPU6050 SDA | **Flying lead** |
| **D33** | soft SCL | Output | MPU6050 SCL | **Flying lead** |
| **D35** | DIR3 | Output | Lift direction | Jumper → J37/J44 |
| **D37** | PWM3 | Output | Lift PWM | Jumper → J37/J44 |
| **D38** | MAGNET SW | Output | Magnet relay K1 | PCB copper |
| **D39** | MD / MTLDTCT | Input | Metal detector | Jumper → J6/J40 |
| **D40** | DIR2 | Output | Left motor DIR | PCB copper |
| **D41** | LM1 | Input | Limit top | Jumper → J43/J44 |
| **D42** | DIR1 | Output | Right motor DIR | PCB copper |
| **D43** | LM2 | Input | Limit bottom | Jumper → J43/J44 |
| **D44** | PWM1 | Output | Right motor PWM | PCB copper |
| **D45** | WARNING_LED | Output | Status LED | Discrete LED |
| **D46** | PWM2 | Output | Left motor PWM | PCB copper |
| **D47** | SIREN | Output | Siren relay | Jumper → J15 |
| **A0**  | BAT_SENSE | Input | Battery divider | Add-on harness |
| **A1–A5** | AP1–AP5 | Input | Proximity | Jumper → J6 |

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

```
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
|----------|------|-------|
| Timer 0 | — | `millis()` — do not reconfigure |
| Timer 3 | D4 | Camera Servo (dedicated 50 Hz PWM via Timer 3, avoids Servo.h Timer 5 hijacking) |
| Timer 5 | D44, D46 | Drive PWMs (hardware PWM, undisturbed) |
| INT2 / INT4 | D21 / D19 | Left ENC A / Right ENC A |
| Soft I2C | D31 / D33 | MPU6050 |

---

## Cytron MDD10A (J36)

| J36 | Signal | Mega |
|-----|--------|------|
| 1 | DIR1 | D42 |
| 2 | PWM1 | D44 (Timer 5 OC5C) |
| 3 | DIR2 | D40 |
| 4 | PWM2 | D46 (Timer 5 OC5A) |
| 5 | GND | GND |

---

## Magnet / Limits / Siren / Gripper

- Magnets: shared relay on **D38** (`MAGNET_SHARED_RELAY`)
- Limits: active HIGH (`LIMIT_SWITCH_ACTIVE_HIGH`) on **D41** (top) and **D43** (bottom)
- Gripper break-beam sensor: digital input pullup on **D25**
- Siren: digital on **D47** (`BUZZER_IS_SIREN_RELAY`)
- Camera Pan Servo: PWM on **D4** (Timer 3)

Full harness details: [WiringManual.md](WiringManual.md).
