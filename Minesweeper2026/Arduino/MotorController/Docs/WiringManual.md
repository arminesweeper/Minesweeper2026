# Minesweeper 2026 — Hardware Wiring & Harness Manual

**Audience:** Assembly technicians, electrical engineers, and commissioning personnel  
**Hardware Platform:** Custom Assiut Robotics PCB (`Robotics_Mine` / `mine_sch.pdf`)  
**Firmware Baseline:** `MotorController` (`Config.h` synchronized to final physical robot wiring)

---

## 1. System Interconnection Architecture

```mermaid
flowchart LR
    BAT[12V Battery Pack Tianneng TNE12-25] --> FUSE[Main Fuse 30A]
    FUSE --> CYTRON[Cytron MDD10A Rev2.0]
    FUSE --> REG5[5V Regulated Logic PSU]
    FUSE --> PWR_RAILS[12V Main / 12V_MG Rails]

    REG5 --> MEGA[Arduino Mega 2560]
    REG5 --> ENC[Encoders Left & Right]
    REG5 --> MPU[MPU6050 6-DOF IMU]

    CYTRON --> MOT_L[Left Drive Motors]
    CYTRON --> MOT_R[Right Drive Motors]

    MEGA -->|USB CDC Serial0 115200| RPI[Raspberry Pi 4 / ROS 2]
    MEGA -->|Hardware Serial2 115200| ESP[ESP8266 WiFi Web Controller]

    MEGA -->|PWM D44/D46, DIR D42/D40| CYTRON
    MEGA -->|PWM D7, DIR D49| LIFT_DRV[Lift Motor Actuator]
    MEGA -->|Relay K1 D38| MAG[Electromagnet Array MAG1-MAG5]
    MEGA -->|Relay K2 D37| SIREN[12V Alert Siren & Strobe]
    
    ENC -->|INT0/INT2 Pins 18-21| MEGA
    MPU -->|Soft-I2C Pins 34, 36| MEGA
    SENS[5x Proximity D23-D31 + Metal D33] --> MEGA
    ULTRA[HC-SR04 Ultrasonic D43, D45] --> MEGA
    LIMITS[Limit Switches D51, D53] --> MEGA
    SERVO[Camera Pan Servo D4] <-- MEGA
```

---

## 2. Power Distribution Rails

| Rail Name | Source | Terminal Header | Filter Capacitance | Subsystems Served | Notes |
| :--- | :--- | :---: | :---: | :--- | :--- |
| **VBAT / 12V** | Battery Positive | `J24` | $2000\,\mu\text{F}$ (C5, C7) | Cytron MDD10A, Relays K1/K2 coils | Fused at 30A near battery terminal |
| **12V_MG** | Magnet Power Bus | `J43` / Internal Bus | $4000\,\mu\text{F}$ (C3, C13) | Electromagnets MAG1–MAG5, Lift motor | Controlled via Relay K1 (2N2222A) |
| **12V_MD** | Sensor Excitation | `J46` | Bulk decoupling | Inductive metal detector, proximity sensors | Stabilized DC supply |
| **5V_LOGIC** | Regulated Buck | `J22` | $2000\,\mu\text{F}$ (C9) + 100nF | Mega 5V rail, Encoders, MPU6050, logic | Clean supply isolated from motors |
| **5V_SERVO** | Dedicated 5V Reg | `J23` / `J13` | $2000\,\mu\text{F}$ (C11, C12) | Camera Pan Servo | Prevents servo inductive kickback |
| **GND** | Battery Negative | Common Stud | Ground Plane | Star ground connection for all modules | Single-point bond near Mega |

### Critical Grounding & Power Rules
1. **Common Star Ground:** Cytron power GND, Arduino Mega GND, and sensor logic GND must bond at a single physical terminal near the battery return to prevent high motor return currents from creating ground bounce on sensitive analog/I²C lines.
2. **Motor Lead Dressing:** Motor output wires must be twisted pairs and routed away from encoder signal harnesses and MPU6050 flying leads.
3. **Decoupling Capacitors:** Ensure bulk electrolytic capacitors (>11,000 µF total across rails) are fully operational and verified before powering inductive loads.

---

## 3. Master Technician Pin Connection Table

| Arduino Mega Pin | Net / Signal Name | Target Device / Header | Physical Connection Type | Operational Function |
| :---: | :---: | :---: | :--- | :--- |
| **D18** | `ENC_R_B` | Right Encoder Phase B | Shield Header `J17` Pin 2 | Quadrature direction sensing |
| **D19** | `ENC_R_A` | Right Encoder Phase A | Shield Header `J17` Pin 3 | External Interrupt `INT2` |
| **D20** | `ENC_L_B` | Left Encoder Phase B | Shield Header `J16` Pin 2 | Quadrature direction sensing |
| **D21** | `ENC_L_A` | Left Encoder Phase A | Shield Header `J16` Pin 3 | External Interrupt `INT0` |
| **D4** | `SERVO` | Camera Pan Servo Signal | Discrete Lead → Header `J13` | Custom 50 Hz PWM via Timer 3 |
| **D7** | `LIFT_PWM` | Lift Actuator Motor Speed | Jumper → Header `J37` / `J44` | Hardware PWM via Timer 4 (OC4B) |
| **D49** | `LIFT_DIR` | Lift Actuator Direction | Jumper → Header `J37` / `J44` | Digital Direction (HIGH/LOW) |
| **D51** | `LIMIT_TOP` | Upper Travel Limit Switch | Jumper → Header `J43` / `J44` | Active-HIGH top travel limit |
| **D53** | `LIMIT_BOT` | Lower Travel Limit Switch | Jumper → Header `J43` / `J44` | Active-HIGH bottom travel limit |
| **D43** | `US_TRIG` | HC-SR04 Ultrasonic Trigger | Gripper Scoop Harness | 10 µs pulse output |
| **D45** | `US_ECHO` | HC-SR04 Ultrasonic Echo | Gripper Scoop Harness | Pulse width proportional to distance |
| **D23** | `PROX1` | Infrared Proximity Sensor 1 | Sensor Bus `J39` / APRX1 | Digital obstacle detection |
| **D25** | `PROX2` | Infrared Proximity Sensor 2 | Sensor Bus `J39` / APRX2 | Digital obstacle detection |
| **D27** | `PROX3` | Infrared Proximity Sensor 3 | Sensor Bus `J39` / APRX3 | Digital obstacle detection |
| **D29** | `PROX4` | Infrared Proximity Sensor 4 | Sensor Bus `J39` / APRX4 | Digital obstacle detection |
| **D31** | `PROX5` | Infrared Proximity Sensor 5 | Sensor Bus `J39` / APRX5 | Digital obstacle detection |
| **D33** | `METAL_DET` | Metal Detector Search Coil | Sensor Header `J40` Pin 1 | Digital mine trigger (Active HIGH) |
| **D34** | `SOFT_SDA` | MPU6050 IMU Data | **Dedicated Flying Lead** | Soft-I2C Serial Data (avoid D31 clash) |
| **D36** | `SOFT_SCL` | MPU6050 IMU Clock | **Dedicated Flying Lead** | Soft-I2C Serial Clock (avoid D33 clash) |
| **D37** | `SIREN` | 12V Siren Relay K2 Driver | Header `J15` Pin 1 (via Q2) | 12V automotive alert horn |
| **D38** | `MAGNET` | Electromagnet Relay K1 | Header `J14` Pin 1 (via Q1) | Common bus for MAG1–MAG5 |
| **D40** | `DIR2` | Left Drive Motor Direction | Cytron MDD10A Header `J36` Pin 3 | High = Forward, Low = Reverse |
| **D42** | `DIR1` | Right Drive Motor Direction| Cytron MDD10A Header `J36` Pin 1 | High = Forward, Low = Reverse |
| **D44** | `PWM1` | Right Drive Motor Speed | Cytron MDD10A Header `J36` Pin 2 | Timer 5 OC5C Hardware PWM |
| **D46** | `PWM2` | Left Drive Motor Speed | Cytron MDD10A Header `J36` Pin 4 | Timer 5 OC5A Hardware PWM |
| **D13** | `WARN_LED` | Diagnostic / Warning LED | Onboard `LED_BUILTIN` | Visual fault indicator |
| **A0** | `BAT_SENSE` | Main Battery Voltage Divider | Resistor Divider Network | Analog read (3.3:1 attenuation) |
| **D0 / D1**| `USB CDC` | Raspberry Pi 4 | USB Cable to Pi USB Port | Primary ROS 2 Command Link |
| **D16 / D17**| `SERIAL2` | ESP8266 WiFi NodeMCU | Jumper to ESP `TX`/`RX` | Web Diagnostic Dashboard Link |

---

## 4. IMU Flying-Lead Harness (Soft-I2C)

> [!WARNING]
> **DO NOT connect the MPU6050 data pins to shield headers J29 or J38.**
> On the fabricated PCB, J29 and J38 connect to Mega pins D20 and D21, which collide directly with the Left Wheel Encoder (ENC1) Phase A and Phase B interrupts.

### Approved Flying-Lead Wiring:
- **MPU6050 VCC:** Connect to 5V (from `J22` Logic Rail or Mega 5V header).
- **MPU6050 GND:** Connect to common system ground.
- **MPU6050 SDA:** Connect directly to Arduino Mega **Pin D34**.
- **MPU6050 SCL:** Connect directly to Arduino Mega **Pin D36**.
- **Pull-Up Resistors:** If using a raw breakout without onboard pull-ups, install $4.7\,\text{k}\Omega$ resistors from D34 to 5V and from D36 to 5V.

---

## 5. Primary Drive Motor Wiring (Cytron MDD10A)

### Header J36 (Logic Interface to Cytron):
- **Pin 1 (DIR1):** Connects to Mega **D42** (Right Motor Direction)
- **Pin 2 (PWM1):** Connects to Mega **D44** (Right Motor PWM, Timer 5 OC5C)
- **Pin 3 (DIR2):** Connects to Mega **D40** (Left Motor Direction)
- **Pin 4 (PWM2):** Connects to Mega **D46** (Left Motor PWM, Timer 5 OC5A)
- **Pin 5 (GND):** Common Ground bond

### Cytron Power & Motor Output Terminals:
- **`POWER + / -`:** Heavy gauge 14 AWG wire from 30A main fused battery rail.
- **`MOTOR 1 (M1A / M1B)`:** Right drive motors (wired in parallel if 4WD).
- **`MOTOR 2 (M2A / M2B)`:** Left drive motors (wired in parallel if 4WD).

---

## 6. Lift Mechanism & Limit Switches Harness

### Lift Motor Driver:
- **PWM Signal:** Mega **Pin D7** (Timer 4 OC4B, speed clamped to 90 / ~35% duty cycle in `LiftConfig::LIFT_PWM_SPEED`).
- **DIR Signal:** Mega **Pin D49**.
- **Motor Leads:** Connected to Terminal **`J28 (LIFT)`**.

### Travel Limit Switches:
- **Top Limit Switch (LIMIT1):** Connect to Mega **Pin D51** (Active-HIGH configuration).
- **Bottom Limit Switch (LIMIT2):** Connect to Mega **Pin D53** (Active-HIGH configuration).
- *Operational note:* In `Config.h`, `#define ENABLE_LIMIT_SWITCHES 0` is currently set for direct manual teleoperation. If automated homing cycles are re-enabled, verify that pressing the upper physical lever asserts logic HIGH on D51.

---

## 7. Electromagnet Array & Alert Siren

### Electromagnets (MAG1–MAG5):
- Outputs: Connectors **`J9, J10, J11, J12, J27`**.
- Gated Power Rail: `12V_MG` via Relay **K1**.
- Control Signal: Mega **Pin D38** drives transistor Q1 (`2N2222A`) to switch Relay K1.
- Flyback Protection: Diode D1 (`1N4007`) across relay coil.
- Firmware Protection: 8-second auto-off timer active to prevent coil thermal overload.

### 12V Automotive Siren:
- Load Output: Connector **`J21`** (Switched 12V).
- Control Signal: Mega **Pin D37** drives transistor Q2 (`2N2222A`) to switch Relay **K2**.
- Flyback Protection: Diode D2 (`1N4007`) across relay coil.
- Logic Polarity: Active-HIGH in final `Config.h` (`#define SIREN_ACTIVE_HIGH 1`).

---

## 8. Sensor Subsystem Wiring

### Optical Proximity Array (APRX1–APRX5):
- Sensors: 5× downward-facing photoelectric proximity sensors.
- Digital Signal Inputs: Mega Pins **`D23, D25, D27, D29, D31`** via header **`J39`**.
- Status Display: 5× blue indicator LEDs on interface board give direct visual feedback.

### Metal Detector Search Coil:
- Sensor Output: Connect to Mega **Pin D33** via header **`J40`**.
- Signal Logic: Digital high indicates electromagnetic target detection.

### Ultrasonic Distance Sensor (HC-SR04):
- **VCC:** 5V Logic Rail.
- **GND:** System GND.
- **Trig:** Arduino Mega **Pin D43**.
- **Echo:** Arduino Mega **Pin D45**.

---

## 9. Communication & Auxiliary Modules

### Raspberry Pi 4 (ROS 2 Primary Brain):
- Connection: Standard USB Type-A to Type-B cable connecting Raspberry Pi USB 3.0 to Arduino Mega USB port.
- Baud Rate: **115200 baud** (Fixed CDC ACM).
- Power: Powered via independent high-efficiency 5V 3A step-down regulator (DO NOT power RPi from Mega 5V pin).

### ESP8266 WiFi Telemetry Module:
- Connection: Connected to Arduino Mega **`Serial2`** port.
  - `ESP8266 TX` ────→ `Mega D17 (RX2)`
  - `ESP8266 RX` ────→ `Mega D16 (TX2)` (through voltage divider or 3.3V logic level shifter)
  - `ESP8266 GND` ───→ `System Common GND`
  - `ESP8266 VCC` ───→ `Regulated 3.3V Rail`
- Baud Rate: **115200 baud**.

### Battery Voltage Sense Divider:
- External Divider Circuit: $23\,\text{k}\Omega$ (R1) and $10\,\text{k}\Omega$ (R2) connected between Battery Positive and Ground.
- Tap Output: Connected to Arduino Mega Analog Pin **`A0`**.
- Attenuation Ratio: 3.3:1 ($14.8\text{ V} \rightarrow 4.48\text{ V}$ safe ADC input).
