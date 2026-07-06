# Arduino Mega 2560 Pin Assignment

**Minesweeper Robot Motor Controller — Complete Pin Map**

---

## Pin Assignment Table

| Pin | Name | Direction | Function | Module | Notes |
|-----|------|-----------|----------|--------|-------|
| **D0** | RX0 | Input | USB Serial RX | SerialProtocol | Reserved for USB (Pi ↔ Arduino) |
| **D1** | TX0 | Output | USB Serial TX | SerialProtocol | Reserved for USB (Pi ↔ Arduino) |
| **D2** | INT4 | Input | Left Encoder Phase A | Encoder | External interrupt (RISING) |
| **D3** | INT5 | Input | Right Encoder Phase A | Encoder | External interrupt (RISING) |
| **D4** | — | Input | Left Encoder Phase B | Encoder | Digital input (pull-up) |
| **D5** | — | Input | Right Encoder Phase B | Encoder | Digital input (pull-up) |
| **D6** | OC4A | Output | Buzzer | Sensors | PWM capable (Timer 4) |
| **D7** | — | Output | Left Motor DIR | MotorDriver | HIGH = forward, LOW = reverse |
| **D8** | — | Output | Lift Motor DIR | LiftController | HIGH = up, LOW = down |
| **D9** | OC2B | Output | Right Motor PWM | MotorDriver | Timer 2 (8-bit, ~490 Hz) |
| **D10** | OC2A | Output | Lift Motor PWM | LiftController | Timer 2 (8-bit, ~490 Hz) |
| **D11** | OC1A | Output | Left Motor PWM | MotorDriver | Timer 1 (16-bit, ~490 Hz) |
| **D12** | — | Output | Right Motor DIR | MotorDriver | HIGH = forward, LOW = reverse |
| **D13** | — | Output | Warning LED | Sensors | Built-in LED, also external LED |
| D14-D19 | — | — | *Available* | — | Reserved for future use |
| **D20** | SDA | I/O | MPU6050 SDA | IMU | I2C data (Wire library) |
| **D21** | SCL | Output | MPU6050 SCL | IMU | I2C clock (Wire library) |
| **D22** | — | Output | Electromagnet 1 | LiftController | HIGH = energize |
| **D23** | — | Output | Electromagnet 2 | LiftController | HIGH = energize |
| **D24** | — | Output | Electromagnet 3 | LiftController | HIGH = energize |
| **D25** | — | Output | Electromagnet 4 | LiftController | HIGH = energize |
| **D26** | — | Output | Electromagnet 5 | LiftController | HIGH = energize |
| **D27** | — | Input | Metal Detector | Sensors | Active LOW (internal pull-up) |
| **D28** | — | Input | Limit Switch — Top | LiftController | Active LOW (internal pull-up) |
| **D29** | — | Input | Limit Switch — Bottom | LiftController | Active LOW (internal pull-up) |
| D30-D53 | — | — | *Available* | — | Reserved for future expansion |
| **A0** | ADC0 | Input | Battery Voltage | Safety | Voltage divider (ratio 3.3:1) |
| **A1** | ADC1 | Input | Proximity Sensor 1 | Sensors | Analog 0-1023 |
| **A2** | ADC2 | Input | Proximity Sensor 2 | Sensors | Analog 0-1023 |
| **A3** | ADC3 | Input | Proximity Sensor 3 | Sensors | Analog 0-1023 |
| **A4** | ADC4 | Input | Proximity Sensor 4 | Sensors | Analog 0-1023 |
| **A5** | ADC5 | Input | Proximity Sensor 5 | Sensors | Analog 0-1023 |
| A6-A15 | — | — | *Available* | — | Reserved for future expansion |

---

## Pin Usage by Module

### Motor Driver (Cytron MDD10A Rev 2.0 — Sign-Magnitude Mode)

```
 MDD10A Channel A (Right Motor)
 ┌────────────────────────────────────┐
 │  PWM1 ◄── D9  (analogWrite 0-255) │
 │  DIR1 ◄── D12 (HIGH=FWD, LOW=REV) │
 │  GND  ◄── Arduino GND             │
 └────────────────────────────────────┘

 MDD10A Channel B (Left Motor)
 ┌────────────────────────────────────┐
 │  PWM2 ◄── D11 (analogWrite 0-255) │
 │  DIR2 ◄── D7  (HIGH=FWD, LOW=REV) │
 │  GND  ◄── Arduino GND             │
 └────────────────────────────────────┘
```

### Wheel Encoders

```
 Right Encoder                Left Encoder
 ┌──────────────┐             ┌──────────────┐
 │ Phase A ──► D3 (INT5)      │ Phase A ──► D2 (INT4)
 │ Phase B ──► D5             │ Phase B ──► D4
 │ VCC    ──► 5V              │ VCC    ──► 5V
 │ GND    ──► GND             │ GND    ──► GND
 └──────────────┘             └──────────────┘
```

### MPU6050 IMU

```
 MPU6050
 ┌──────────────┐
 │ SDA   ──► D20 (with 4.7kΩ pull-up to 3.3V)
 │ SCL   ──► D21 (with 4.7kΩ pull-up to 3.3V)
 │ VCC   ──► 3.3V
 │ GND   ──► GND
 │ AD0   ──► GND (I2C address = 0x68)
 │ INT   ──► (not connected)
 └──────────────┘
```

### Lift Mechanism

```
 Lift Motor Driver
 ┌────────────────────────────────────┐
 │  PWM  ◄── D10 (analogWrite 0-255) │
 │  DIR  ◄── D8  (HIGH=UP, LOW=DOWN) │
 │  GND  ◄── Arduino GND             │
 └────────────────────────────────────┘

 Limit Switches (Normally Open, Active LOW)
 ┌────────────────────────────┐
 │  Top    ──► D28 (INPUT_PULLUP)    │
 │  Bottom ──► D29 (INPUT_PULLUP)    │
 │  Common ──► GND                   │
 └────────────────────────────┘
```

### Electromagnets

```
 Via MOSFET/relay driver modules:
 ┌────────────────────────┐
 │  Magnet 1 ◄── D22      │
 │  Magnet 2 ◄── D23      │
 │  Magnet 3 ◄── D24      │
 │  Magnet 4 ◄── D25      │
 │  Magnet 5 ◄── D26      │
 │  All GND  ◄── GND      │
 └────────────────────────┘
 Note: Magnets draw significant current.
 Drive via MOSFET or relay, NOT directly from Arduino pins.
```

### Sensors

```
 Metal Detector (Digital)
 ┌────────────────────────┐
 │  Signal ──► D27 (INPUT_PULLUP)    │
 │  VCC    ──► 5V                    │
 │  GND    ──► GND                   │
 └────────────────────────┘
 Active LOW: pin reads LOW when metal detected.

 Proximity Sensors (Analog)
 ┌────────────────────────┐
 │  Sensor 1 ──► A1       │
 │  Sensor 2 ──► A2       │
 │  Sensor 3 ──► A3       │
 │  Sensor 4 ──► A4       │
 │  Sensor 5 ──► A5       │
 │  VCC      ──► 5V       │
 │  GND      ──► GND      │
 └────────────────────────┘
```

### Indicators

```
 Buzzer (PWM)
 ┌────────────────────────┐
 │  Signal ◄── D6 (PWM)   │
 │  GND    ◄── GND        │
 └────────────────────────┘
 Uses tone()/noTone() for frequency control.

 Warning LED
 ┌────────────────────────┐
 │  Anode  ◄── D13 (via 220Ω resistor) │
 │  Cathode ◄── GND                    │
 └────────────────────────┘
```

### Battery Monitoring

```
 Battery Voltage Divider
 ┌────────────────────────────────────┐
 │  VBAT ──┤R1├──┬──┤R2├── GND       │
 │                │                    │
 │                └──► A0              │
 │                                     │
 │  R1 + R2 / R2 = 3.3 (divider ratio)│
 │  Example: R1 = 23kΩ, R2 = 10kΩ     │
 │                                     │
 │  Max input: 16.8V × (10/33) = 5.09V│
 └────────────────────────────────────┘
```

---

## Power Connections

| Rail | Source | Consumers |
|------|--------|-----------|
| 5V | Arduino USB or Vin regulator | Encoders, proximity sensors, metal detector |
| 3.3V | Arduino 3.3V regulator | MPU6050 |
| GND | Common ground | All devices |
| VBAT | Battery (11.1V-16.8V nominal) | Motor driver, lift motor, electromagnets |

> **Critical:** All GND connections must share a common ground plane.  Motor driver GND must be connected to Arduino GND.

---

## Timer Allocation

| Timer | Pins | Usage | Default Freq |
|-------|------|-------|--------------|
| Timer 0 | D4, D13 | `millis()`, `delay()`, `micros()` | 976 Hz |
| Timer 1 | D11, D12 | Left motor PWM | ~490 Hz |
| Timer 2 | D9, D10 | Right motor PWM, Lift motor PWM | ~490 Hz |
| Timer 3 | D2, D3, D5 | (Available — encoder pins are digital input) | — |
| Timer 4 | D6, D7, D8 | Buzzer (`tone()`), motor DIR (digital only) | — |
| Timer 5 | D44, D45, D46 | (Available) | — |

> **Note:** Timer 0 is used by Arduino core for timing functions.  Do not modify Timer 0 prescaler.

---

## Available Pins (Expansion)

| Pin Range | Count | Suggested Use |
|-----------|-------|---------------|
| D14-D19 | 6 | Additional serial ports (Serial1/2/3) |
| D30-D53 | 24 | Additional sensors, SPI devices |
| A6-A15 | 10 | Additional analog sensors, temperature |

---

## JST Connector Assignments (Suggested)

| Connector | Label | Pins | Purpose |
|-----------|-------|------|---------|
| J1 | RIGHT_MOTOR | D9, D12, GND | Right motor driver |
| J2 | LEFT_MOTOR | D11, D7, GND | Left motor driver |
| J3 | RIGHT_ENC | D3, D5, 5V, GND | Right encoder |
| J4 | LEFT_ENC | D2, D4, 5V, GND | Left encoder |
| J5 | IMU | D20, D21, 3.3V, GND | MPU6050 |
| J6 | LIFT | D10, D8, D28, D29, GND | Lift motor + switches |
| J7 | MAGNETS | D22-D26, GND | Electromagnets |
| J8 | PROXIMITY | A1-A5, 5V, GND | Proximity sensors |
| J9 | METAL | D27, 5V, GND | Metal detector |
| J10 | BATT | A0, GND | Battery sense |
