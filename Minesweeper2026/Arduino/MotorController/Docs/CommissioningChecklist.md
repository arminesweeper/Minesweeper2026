# Hardware Commissioning Checklist

**Audience:** Assembly technicians and commissioning engineers  
**Hardware Baseline:** As-built robot wiring (synchronized with `Config.h` and `Docs/Pinout.md`)

Use this checklist before competition trials and field deployment. Check all boxes in sequence. Do not skip Critical safety checks.

---

## A. Continuity & Harness (Critical — No Short Circuits)

- [ ] Battery disconnected; large filter capacitors completely discharged.
- [ ] Left wheel encoder (ENC1) Phase B continuity to Mega **D20** (via J16 Pin 2).
- [ ] Left wheel encoder (ENC1) Phase A continuity to Mega **D21** (via J16 Pin 3, INT0).
- [ ] Right wheel encoder (ENC2) Phase B continuity to Mega **D18** (via J17 Pin 2).
- [ ] Right wheel encoder (ENC2) Phase A continuity to Mega **D19** (via J17 Pin 3, INT2).
- [ ] MPU6050 SDA and SCL **NOT** connected to shield headers J29/J38 (avoids D20/D21 clash).
- [ ] MPU6050 Soft-I2C flying leads verified: **SDA ↔ Mega D34**, **SCL ↔ Mega D36**.
- [ ] Soft-I2C pull-ups present (4.7 kΩ pull-ups to 5V verified on SDA/SCL lines).
- [ ] Cytron MDD10A logic header (J36) continuity verified:
  - Mega **D44** ↔ PWM1 (Right Speed, Timer 5 OC5C)
  - Mega **D42** ↔ DIR1 (Right Direction)
  - Mega **D46** ↔ PWM2 (Left Speed, Timer 5 OC5A)
  - Mega **D40** ↔ DIR2 (Left Direction)
- [ ] Magnet Relay K1 drive continuity: Mega **D38** ↔ Transistor Q1 base.
- [ ] Siren Relay K2 drive continuity: Mega **D37** ↔ Transistor Q2 base.
- [ ] Lift motor driver jumpers installed: **PWM ↔ Mega D7** (Timer 4 OC4B), **DIR ↔ Mega D49**.
- [ ] Travel limit switch leads verified: **Top ↔ Mega D51**, **Bottom ↔ Mega D53**.
- [ ] Ultrasonic sensor leads verified: **Trig ↔ Mega D43**, **Echo ↔ Mega D45**.
- [ ] Camera Pan Servo lead verified: **Signal ↔ Mega D4** (Timer 3).
- [ ] Proximity sensor array inputs verified: **D23, D25, D27, D29, D31**.
- [ ] Metal detector search coil input verified: **Mega D33**.
- [ ] Multimeter continuity check: No short circuit between 12V rails and 5V or GND.
- [ ] Star ground integrity: Verified common GND across Battery, Mega, Cytron, and sensor boards.

---

## B. Power Rails & Fusing

- [ ] 30A main inline fuse installed on battery positive lead.
- [ ] 5V_LOGIC rail stable between 4.9 V and 5.2 V under Arduino Mega idle load.
- [ ] 5V_SERVO rail present and isolated from logic power (if servo installed).
- [ ] 12V Main power rail present at terminal `J24` (matches battery voltage, 12.0 V – 13.2 V).
- [ ] 12V_MD sensor rail present at terminal `J46`.
- [ ] 12V_MG magnet rail present at terminal `J43` / magnet section.
- [ ] Correct polarity verified on all power terminals before energizing.

---

## C. Firmware Boot & Diagnostics

- [ ] Arduino IDE / CLI configured for: **Arduino Mega 2560** (ATmega2560).
- [ ] Firmware compiles and uploads cleanly without errors.
- [ ] Serial monitor at 115200 baud outputs ready banner and telemetry packets.
- [ ] Onboard diagnostic LED (**D13**) displays normal heartbeat without watchdog resets.

---

## D. IMU (Soft-I2C on D34/D36)

- [ ] MPU6050 powered via clean 5V and connected to flying leads **D34 (SDA)** and **D36 (SCL)**.
- [ ] MPU6050 detected by firmware (`WHO_AM_I` register returns 0x68).
- [ ] Telemetry confirms pitch, roll, and yaw angles respond smoothly when tilting the robot.
- [ ] IMU data remains clean and jitter-free while spinning the left drive wheel by hand (proves isolation from D20/D21 encoder interrupts).

---

## E. Encoders & Odometry Feedback

- [ ] Manually spin right wheel forward → Telemetry registers positive velocity and accumulating ticks (~385 PPR).
- [ ] Manually spin right wheel in reverse → Telemetry registers negative velocity.
- [ ] Manually spin left wheel forward → Telemetry registers positive velocity.
- [ ] Manually spin left wheel in reverse → Telemetry registers negative velocity.
- [ ] Zero crosstalk: Spinning right wheel produces zero tick count on left wheel channel.

---

## F. Primary Drive Motors (Jack Stands / Unloaded)

- [ ] Chassis safely elevated on test stands with wheels off the ground.
- [ ] Send small forward command (`rp01.00,lp01.00,`) → Both drive wheels rotate forward.
- [ ] Send reverse command (`rn01.00,ln01.00,`) → Both drive wheels rotate in reverse.
- [ ] Disconnect USB cable or stop sending commands → Watchdog halts both motors within 1.0–1.5 seconds.
- [ ] Verify Cytron status LEDs (DL1–DL4) illuminate cleanly without excessive heat or stall current.

---

## G. Lift Mechanism & Limit Switches

- [ ] Lift motor connected to Terminal `J28` with PWM on **D7** and DIR on **D49**.
- [ ] Send raise command (`CLIFT:UP\n` or `gp01.00,`) → Lift mechanism elevates smoothly at safe speed (`LIFT_PWM_SPEED = 90`).
- [ ] Send lower command (`CLIFT:DN\n`) → Lift descends smoothly.
- [ ] Manually depress Upper Limit Switch (**D51**) → Lift stops immediately (when `ENABLE_LIMIT_SWITCHES 1`).
- [ ] Manually depress Lower Limit Switch (**D53**) → Lift stops immediately (when `ENABLE_LIMIT_SWITCHES 1`).
- [ ] Lift stall timeout triggers FAULT safely if limit switch is disconnected.

---

## H. Electromagnet Array & Siren Relay

- [ ] Send magnet enable command (`CMAG:ALL:ON\n` or `gp01.00`) → Relay K1 clicks; all 5 magnet headers (`MAG1–MAG5`) energize.
- [ ] Verify 8-second auto-off timer: Magnets automatically de-energize after timeout to prevent overheating.
- [ ] Send siren test command (`CSIREN:ON\n` / Pattern ALERT) → Relay K2 clicks; 12V siren sounds.
- [ ] Send siren silence command (`CSIREN:OFF\n`) → Siren silences immediately.

---

## I. Sensor Array & Detection Logic

- [ ] Wave target in front of each proximity sensor (APRX1–APRX5 on **D23, D25, D27, D29, D31**) → Respective blue LED lights up on interface board.
- [ ] Bring metallic target near search coil (**D33**) → Metal detector digital bit transitions to HIGH in telemetry.
- [ ] Place sample surface mine in gripper scoop → HC-SR04 ultrasonic sensor (**D43/D45**) detects target within calibrated scoop distance.
- [ ] Camera Pan Servo responds to position commands (`sp90.00,`, `sp0.00,`, `sp180.00,`) on **D4** smoothly.

---

## J. Battery Voltage Monitor

- [ ] Voltage divider harness connected to Battery Positive and grounded at common stud.
- [ ] Sense lead securely connected to Mega Analog Pin **`A0`**.
- [ ] Measured multimeter battery voltage matches telemetry reported voltage within $\pm 0.1\text{ V}$.
- [ ] Safety cutoffs verified: System generates low voltage alert at 10.8V and critical e-stop at 9.6V.

---

## K. High-Level Communications (ROS 2 & ESP8266)

- [ ] **USB Link:** Raspberry Pi detects Mega on `/dev/ttyUSB0` or `/dev/ttyACM0` at 115200 baud.
- [ ] ROS 2 hardware interface publishes `/odom`, `/joint_states`, and accepts `/cmd_vel` without dropped packets.
- [ ] **WiFi Link:** ESP8266 boots, connects on `Serial2` (**D16/D17**), and serves Web Dashboard at `http://192.168.4.1`.
- [ ] Real-time battery status, lift position, and sensor indicators update dynamically in the Web Dashboard.

---

## L. Field Commissioning Sign-Off

| Inspection Role | Engineer Name | Date | Verification Status |
| :--- | :--- | :---: | :---: |
| **Electrical / Harness Engineer** | | | [ ] PASS  [ ] FAIL |
| **Firmware & Control Engineer** | | | [ ] PASS  [ ] FAIL |
| **Team Technical Lead** | | | [ ] PASS  [ ] FAIL |

**Commissioning Notes & Field Calibration Values:**
```
Right Wheel PID: Kp = 11.5, Ki = 7.5, Kd = 0.1, Kff = 0.5
Left Wheel PID:  Kp = 12.8, Ki = 8.3, Kd = 0.1, Kff = 0.5
Battery Open Circuit Voltage: _________ V
Field Notes: ___________________________________________________________
```
