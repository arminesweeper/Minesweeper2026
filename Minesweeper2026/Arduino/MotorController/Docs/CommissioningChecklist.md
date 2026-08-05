# Hardware Commissioning Checklist

Use this before competition / field deployment. Check boxes in order. Do not skip Critical items.

---

## A. Continuity & Harness (Critical — No PCB Cuts)

- [ ] Battery disconnected; capacitors discharged
- [ ] Mega D20 continuity to ENC1 B1 (J16) — **expected**
- [ ] Mega D21 continuity to ENC1 A1 (J16) — **expected**
- [ ] Mega D18 ↔ ENC2 B2; D19 ↔ ENC2 A2
- [ ] MPU SDA/SCL **not** connected to J29/J38 data pins
- [ ] MPU SDA ↔ Mega D30; MPU SCL ↔ Mega D31 (flying leads)
- [ ] Soft-I2C pull-ups present (on module or discrete 4.7 kΩ)
- [ ] Mega D44/D42/D46/D40 ↔ J36 PWM1/DIR1/PWM2/DIR2
- [ ] Mega D38 ↔ J14 magnet drive
- [ ] Lift/limit/siren/metal jumpers installed per WiringManual
- [ ] No short between 12V and 5V / GND
- [ ] Common GND: Mega ↔ Cytron ↔ sensor boards

---

## B. Power Rails

- [ ] 5V_LOGIC stable 4.9–5.2 V under Mega idle load
- [ ] 5V_SERVO present (if servo used)
- [ ] 12V rail present at J24 (within battery range)
- [ ] 12V_MD present at J46 when intended
- [ ] 12V_MG present at magnet/lift section
- [ ] Fuse installed on battery positive
- [ ] Polarity of all PSU inputs verified

---

## C. Firmware Boot

- [ ] Correct board: Arduino Mega 2560 selected
- [ ] Firmware flashes without error
- [ ] Serial 115200 shows `MC:READY` (or project ready banner)
- [ ] No continuous watchdog resets

---

## D. IMU (Soft I2C)

- [ ] MPU on D30/D31 flying leads (not shield J29 data)
- [ ] MPU6050 detected (WHO_AM_I / no IMU fault)
- [ ] Yaw/pitch/roll change when tilted
- [ ] IMU still OK while left encoder is spun (isolation check)

---

## E. Encoders

- [ ] Spin right wheel forward → positive right velocity in telemetry
- [ ] Spin right wheel reverse → negative right velocity
- [ ] Spin left wheel forward → positive left velocity (after invert flag)
- [ ] No count activity on left when right spun (crosstalk)
- [ ] IMU still OK while spinning encoders (proves I2C/encoder isolation)

---

## F. Drive Motors (Unloaded / jack stands)

- [ ] Command small `rp` / `lp` velocities
- [ ] Right motor direction matches command (else set invert flag)
- [ ] Left motor direction matches command
- [ ] Both stop on command timeout / e-stop
- [ ] No Cytron overtemperature / abnormal current at stall

---

## G. Lift & Limits

- [ ] Jumpers D10/D8/D28/D29 installed
- [ ] Raise command moves lift up
- [ ] Top limit (LM1/D28) stops raise → RAISED
- [ ] Lower command moves lift down
- [ ] Bottom limit (LM2/D29) stops lower
- [ ] Stall timeout enters FAULT if switch disconnected
- [ ] clearFault recovers

---

## H. Magnets & Siren

- [ ] D38 HIGH closes K1 / energizes magnets (as designed)
- [ ] D38 LOW releases
- [ ] Magnet ROS mask bits all map to shared relay behavior
- [ ] D37 HIGH sounds siren; LOW silences
- [ ] Pattern ALERT/ALARM toggles audible output

---

## I. Sensors

- [ ] A1–A5 jumpers to J6 AP1–AP5
- [ ] Proximity ADC changes with target
- [ ] Threshold triggers match field needs (tune `PROXIMITY_THRESHOLD`)
- [ ] Metal detector D27 goes active on metal; debounced in telemetry
- [ ] Mine-detect siren pattern engages on rising edge

---

## J. Battery Monitor (if fitted)

- [ ] Divider on A0 installed (ratio 3.3)
- [ ] Reported mV within ~5% of multimeter
- [ ] Low / critical thresholds trigger expected safety behavior

---

## K. ROS 2 / Pi Link

- [ ] USB enumerates on Pi
- [ ] Velocity commands accepted
- [ ] Telemetry / odometry / IMU topics (or serial streams) update
- [ ] E-stop command stops motors within one control period

---

## L. Integrated Motion

- [ ] Straight-line drive ~1 m
- [ ] Spin in place
- [ ] PID stable (no hunting at constant velocity)
- [ ] Slip / encoder fault behavior acceptable
- [ ] Full mission dry-run without motor power faults

---

## Sign-off

| Role | Name | Date | Result |
|------|------|------|--------|
| Technician | | | Pass / Fail |
| Firmware eng. | | | Pass / Fail |
| Team lead | | | Pass / Fail |

Notes:

_________________________________________________________________

_________________________________________________________________
