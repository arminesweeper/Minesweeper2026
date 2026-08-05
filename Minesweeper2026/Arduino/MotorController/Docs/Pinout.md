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
| **D30** | soft SDA | I/O | MPU6050 SDA | **Flying lead** |
| **D31** | soft SCL | Output | MPU6050 SCL | **Flying lead** |
| **D8** | DIR3 | Output | Lift direction | Jumper → J37/J44 |
| **D10** | PWM3 | Output | Lift PWM | Jumper → J37/J44 |
| **D27** | MD / MTLDTCT | Input | Metal detector | Jumper → J6/J40 |
| **D28** | LM1 | Input | Limit top (active HIGH) | Jumper → J43/J44 |
| **D29** | LM2 | Input | Limit bottom (active HIGH) | Jumper → J43/J44 |
| **D35** | WARNING_LED | Output | Status LED | Discrete LED |
| **D36** | SERVO | Output | Optional servo | PCB copper |
| **D37** | SIREN | Output | Siren relay | Jumper → J15 |
| **D38** | MAGNET SW | Output | Magnet relay K1 | PCB copper |
| **D40** | DIR2 | Output | Left motor DIR | PCB copper |
| **D42** | DIR1 | Output | Right motor DIR | PCB copper |
| **D44** | PWM1 | Output | Right motor PWM | PCB copper |
| **D46** | PWM2 | Output | Left motor PWM | PCB copper |
| **A0** | BAT_SENSE | Input | Battery divider | Add-on harness |
| **A1–A5** | AP1–AP5 | Input | Proximity | Jumper → J6 |

USB Serial (D0/D1) reserved for Pi ↔ Arduino.

---

## Encoder / I2C Conflict — Solved Without PCB Edits

As fabricated, ENC1 uses Mega **D20/D21**, which are also the hardware `Wire` pins.

**Solution (firmware + harness):**

1. Keep encoders on D18–D21 exactly as the shield routes them.
2. Enable `IMU_USE_SOFT_I2C` (bit-bang on **D30/D31**).
3. Wire the MPU6050 with **flying leads** to D30 (SDA), D31 (SCL), 5V, GND.
4. **Do not** connect MPU SDA/SCL to shield connectors J29/J38 (those pads share D20/D21 with ENC1 via the Mega).

```
Shield copper (unchanged):
  D21 ← A1 (left enc)     D20 ← B1 (left enc)
  D19 ← A2 (right enc)    D18 ← B2 (right enc)

Flying lead (not via J29 data):
  MPU SDA → Mega D30
  MPU SCL → Mega D31
```

---

## Timer / Interrupt Allocation

| Resource | Pins | Usage |
|----------|------|-------|
| Timer 0 | — | `millis()` — do not reconfigure |
| Timer 2 | D10 | Lift PWM |
| Timer 5 | D44, D46 | Drive PWMs |
| INT2 / INT4 | D21 / D19 | Left ENC A / Right ENC A |
| Soft I2C | D30/D31 | MPU6050 |

---

## Cytron MDD10A (J36)

| J36 | Signal | Mega |
|-----|--------|------|
| 1 | DIR1 | D42 |
| 2 | PWM1 | D44 |
| 3 | DIR2 | D40 |
| 4 | PWM2 | D46 |
| 5 | GND | GND |

---

## Magnet / Limits / Siren

- Magnets: shared relay on **D38** (`MAGNET_SHARED_RELAY`)
- Limits: active HIGH (`LIMIT_SWITCH_ACTIVE_HIGH`)
- Siren: digital on **D37** (`BUZZER_IS_SIREN_RELAY`)

Full harness details: [WiringManual.md](WiringManual.md).
