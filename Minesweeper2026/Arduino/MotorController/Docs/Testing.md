# Testing Plan

**Minesweeper Robot Motor Controller — Comprehensive Test Procedures**

---

## Test Categories

| Category | Purpose | Environment |
|----------|---------|-------------|
| Unit Tests | Verify individual module logic | Serial monitor / simulation |
| Integration Tests | Verify module interactions | Bench with hardware |
| Hardware Tests | Verify physical connections | Full robot |
| Competition Tests | Verify competition readiness | Competition arena |
| Fault Injection | Verify safety systems | Controlled failure |

---

## Unit Tests

### UT-01: Serial Protocol Parser

**Objective:** Verify correct parsing of velocity commands.

| # | Input | Expected Right | Expected Left | Notes |
|---|-------|----------------|---------------|-------|
| 1 | `rp02.50,lp01.30,` | +2.50 | +1.30 | Normal forward |
| 2 | `rn01.00,ln01.00,` | -1.00 | -1.00 | Normal reverse |
| 3 | `rp00.00,lp00.00,` | 0.00 | 0.00 | Stop |
| 4 | `rp05.00,ln02.00,` | +5.00 | -2.00 | Differential turn |
| 5 | `rp10.00,lp10.00,` | +10.00 | +10.00 | Max velocity |
| 6 | `rp15.00,lp15.00,` | +10.00 | +10.00 | Clamped to max |
| 7 | `rp0.50,lp0.50,` | +0.50 | +0.50 | Short value |
| 8 | `rp99.999,lp99.999,` | Clamped | Clamped | Long value |
| 9 | `garbage` | No change | No change | Invalid input |
| 10 | `rp02.50,` | Partial | N/A | Incomplete pair |
| 11 | `rx02.50,lp01.30,` | No change | No change | Invalid direction |
| 12 | (empty) | No change | No change | No data |

**Procedure:**
1. Flash firmware.
2. Open serial monitor at 115200.
3. Send each test input.
4. Observe telemetry response.
5. Verify motor behavior matches expected direction/speed.

### UT-02: Extended Command Parser

| # | Input | Expected Action |
|---|-------|-----------------|
| 1 | `CLIFT:UP\n` | Lift starts raising |
| 2 | `CLIFT:DN\n` | Lift starts lowering |
| 3 | `CLIFT:STOP\n` | Lift stops |
| 4 | `CMAG:1:ON\n` | Magnet 1 energized |
| 5 | `CMAG:ALL:OFF\n` | All magnets off |
| 6 | `CBUZZ:ALERT\n` | Buzzer alert pattern |
| 7 | `CRESET\n` | All subsystems reset |
| 8 | `CDIAG\n` | Diagnostics sent |
| 9 | `CINVALID\n` | Error message sent |

### UT-03: PID Controller

| # | Test | Expected Behavior |
|---|------|-------------------|
| 1 | Step response 0→2 rad/s | Reaches setpoint within 500 ms, < 10% overshoot |
| 2 | Step response 2→0 rad/s | Reaches zero within 300 ms |
| 3 | Anti-windup (stall wheel) | Integral term bounded; fast recovery on release |
| 4 | Negative velocity | Correct direction reversal |
| 5 | Zero setpoint | Output = 0 (not oscillating around zero) |

### UT-04: Velocity Ramping

| # | Test | Expected Behavior |
|---|------|-------------------|
| 1 | 0→5 rad/s step | Ramps over ~400 ms (15 rad/s² accel) |
| 2 | 5→0 rad/s step | Ramps over ~250 ms (20 rad/s² decel) |
| 3 | 5→-5 rad/s step | Decelerates to 0, then accelerates to -5 |
| 4 | Rapid command changes | Profiled velocity smoothly follows |

### UT-05: Odometry

| # | Test | Expected Behavior |
|---|------|-------------------|
| 1 | Both wheels forward equal speed | X increases, Y ≈ 0, θ ≈ 0 |
| 2 | Right forward, left reverse (equal) | Rotation in place, X ≈ 0, Y ≈ 0, θ increases |
| 3 | Only right wheel forward | Arc to left, θ increases |
| 4 | Stop from motion | Position holds, no drift |
| 5 | Long straight run (1m) | X ≈ 1.0 ± 0.05 m |

### UT-06: IMU

| # | Test | Expected Behavior |
|---|------|-------------------|
| 1 | Stationary on flat surface | Yaw ≈ 0 (after calibration), Pitch ≈ 0, Roll ≈ 0 |
| 2 | Rotate 90° by hand | Yaw changes by ~90° |
| 3 | Tilt forward | Pitch changes, Roll ≈ 0 |
| 4 | Tilt sideways | Roll changes, Pitch ≈ 0 |
| 5 | Disconnect I2C | `imu.hasError()` returns true |

### UT-07: Lift Controller

| # | Test | Expected Behavior |
|---|------|-------------------|
| 1 | Raise command | Motor runs, stops at top limit switch |
| 2 | Lower command | Motor runs, stops at bottom limit switch |
| 3 | Raise without top switch | Stall timeout → FAULT after 5 seconds |
| 4 | Lower without bottom switch | Stall timeout → FAULT after 5 seconds |
| 5 | Double raise command | Ignored (already raising/raised) |

### UT-08: Sensors

| # | Test | Expected Behavior |
|---|------|-------------------|
| 1 | Metal near detector | `M:1` in telemetry, buzzer MINE_DETECT |
| 2 | No metal | `M:0` in telemetry |
| 3 | Object near proximity 1 | P[0] value increases |
| 4 | Buzzer ALERT pattern | 200ms on/off beeping |
| 5 | LED HEARTBEAT pattern | Double-pulse blink |

---

## Integration Tests

### IT-01: Full Command-Response Loop

**Objective:** End-to-end test from serial command to motor output to telemetry response.

**Procedure:**
1. Send `rp02.00,lp02.00,` at 10 Hz.
2. Verify both wheels rotate forward at ~2 rad/s.
3. Verify telemetry shows `rp2.0xx,lp2.0xx,` within 500 ms.
4. Verify odometry shows X increasing.
5. Send `rp00.00,lp00.00,`.
6. Verify wheels stop within 300 ms.

### IT-02: Safety + Motor Integration

**Procedure:**
1. Send velocity commands at 10 Hz.
2. Stop sending commands.
3. Verify motors stop after 1500 ms (command timeout).
4. Verify `SAFETY: Command timeout` message.
5. Resume sending commands.
6. Verify motors resume.

### IT-03: Lift + Magnet Integration

**Procedure:**
1. Send `CLIFT:UP\n`.
2. Wait for `L:RAISED` in telemetry.
3. Send `CMAG:ALL:ON\n`.
4. Verify `L:RAISED,1F` in telemetry.
5. Send `CLIFT:DN\n`.
6. Wait for `L:IDLE` in telemetry.
7. Send `CMAG:ALL:OFF\n`.
8. Verify magnets de-energize.

### IT-04: IMU + Odometry Cross-Check

**Procedure:**
1. Command robot to rotate in place (`rp01.00,ln01.00,`).
2. Compare odometry θ with IMU yaw.
3. Verify values track within ±5° over 360° rotation.

### IT-05: Sensor + Safety Integration

**Procedure:**
1. Trigger metal detector during normal operation.
2. Verify `M:1` telemetry without interrupting motor control.
3. Trigger proximity sensors.
4. Verify `P:...` telemetry updates.

---

## Hardware Tests

### HW-01: Motor Direction Verification

| Test | Expected |
|------|----------|
| Right motor, positive command | Wheel turns forward (robot moves forward) |
| Right motor, negative command | Wheel turns reverse |
| Left motor, positive command | Wheel turns forward |
| Left motor, negative command | Wheel turns reverse |

If direction is wrong: toggle `inverted` flag in MotorDriver constructor.

### HW-02: Encoder Direction Verification

| Test | Expected |
|------|----------|
| Spin right wheel forward by hand | Positive velocity in telemetry |
| Spin right wheel reverse by hand | Negative velocity in telemetry |
| Spin left wheel forward by hand | Positive velocity |
| Spin left wheel reverse by hand | Negative velocity |

If direction is wrong: toggle `direction_inverted` flag in Encoder constructor.

### HW-03: Limit Switch Verification

| Test | Expected |
|------|----------|
| Press top limit switch | Lift motor stops if raising |
| Press bottom limit switch | Lift motor stops if lowering |
| Both switches unpressed | Lift motor free to move |

### HW-04: Electromagnet Verification

| Test | Expected |
|------|----------|
| `CMAG:1:ON` | Magnet 1 attracts metal |
| `CMAG:1:OFF` | Magnet 1 releases |
| `CMAG:ALL:ON` | All 5 magnets attract |
| `CMAG:ALL:OFF` | All 5 magnets release |

### HW-05: Battery Voltage Calibration

**Procedure:**
1. Measure actual battery voltage with multimeter.
2. Read `readBatteryVoltage()` value from diagnostics.
3. Calculate correction factor.
4. Adjust `VOLTAGE_DIVIDER_RATIO` in Config.h if needed.

---

## Competition Tests

### CT-01: Full Mission Simulation

**Objective:** Complete a simulated mine detection and removal mission.

**Procedure:**
1. Power on robot.
2. Verify `MC:READY` message.
3. Navigate to mine location (via ROS 2 commands).
4. Verify metal detector triggers `M:1`.
5. Stop robot.
6. Lower lift and energize magnets.
7. Pick up mine.
8. Raise lift.
9. Navigate to disposal area.
10. Lower lift and de-energize magnets.
11. Return to start.

**Pass criteria:** Complete mission without faults, timeouts, or manual intervention.

### CT-02: Endurance Test

**Objective:** Verify reliability over extended operation.

**Procedure:**
1. Run robot with periodic velocity commands for 30 minutes.
2. Monitor diagnostics for loop timing degradation.
3. Monitor battery voltage.
4. Verify no timeout events.
5. Verify no memory leaks (SRAM stays stable).
6. Verify no encoder count discontinuities.

### CT-03: Obstacle Course

**Procedure:**
1. Navigate through a course with tight turns.
2. Verify proximity sensors detect obstacles.
3. Verify odometry tracking matches actual path (within ±10%).

### CT-04: Rapid Start-Stop

**Procedure:**
1. Rapidly send start/stop commands (10 cycles).
2. Verify no motor driver overheating.
3. Verify PID remains stable.
4. Verify no encoder errors.

---

## Fault Injection Tests

### FI-01: Encoder Disconnect

**Procedure:**
1. Run motors at steady velocity.
2. Disconnect one encoder cable.
3. **Expected:** PID output saturates (no feedback), velocity telemetry shows 0 for that wheel.
4. **Verify:** Safety system detects encoder fault (future enhancement).

### FI-02: Serial Loss (USB Disconnect)

**Procedure:**
1. Run motors at steady velocity.
2. Disconnect USB cable from Raspberry Pi.
3. **Expected:** Command timeout triggers after 1500 ms, motors stop.
4. Reconnect USB.
5. Send commands.
6. **Expected:** Motors resume after first valid command.

### FI-03: Motor Stall

**Procedure:**
1. Run motors at low velocity (1 rad/s).
2. Physically stall one wheel (hold it).
3. **Expected:** PID output saturates, anti-windup limits integral accumulation.
4. Release wheel.
5. **Expected:** Motor recovers to setpoint within 500 ms without overshoot.

### FI-04: Power Interruption

**Procedure:**
1. Run robot normally.
2. Briefly disconnect power (< 100 ms brown-out).
3. **Expected:** Arduino resets, boots normally, sends `MC:READY`.
4. **Verify:** Watchdog flag may or may not be set depending on boot sequence.

### FI-05: Emergency Stop

**Procedure:**
1. Run motors at full speed.
2. Send `CESTOP\n`.
3. **Expected:** Motors stop immediately (< 10 ms), state = ESTOP_MANUAL.
4. Send `CCLEAR\n`.
5. **Expected:** State returns to ACTIVE, motors remain stopped until next velocity command.

### FI-06: Watchdog Reset

**Procedure:**
1. Inject an infinite loop (modify firmware temporarily):
   ```cpp
   while(true) { } // Hangs loop()
   ```
2. **Expected:** WDT resets Arduino after 2 seconds.
3. **Expected:** On reboot, diagnostics show `watchdog_reset = true`.

### FI-07: I2C Bus Lockup (IMU)

**Procedure:**
1. Disconnect MPU6050 SDA wire while running.
2. **Expected:** IMU error flag set, `FAULT_IMU` state.
3. **Expected:** Motor control continues (degraded mode, no IMU data).
4. Reconnect SDA.
5. **Expected:** IMU recovers, fault clears.

### FI-08: Lift Stall

**Procedure:**
1. Block lift mechanism so limit switch is never reached.
2. Send `CLIFT:UP\n`.
3. **Expected:** Motor runs for `STALL_TIMEOUT_MS` (5 seconds), then stops.
4. **Expected:** Lift state = FAULT.
5. **Verify:** Error message sent via serial.

### FI-09: Battery Low Voltage

**Procedure:**
1. Use a variable power supply.
2. Reduce voltage below 10.8V.
3. **Expected:** `FAULT_BATTERY` state, motors stopped.
4. Restore voltage above 10.8V.
5. **Expected:** State returns to ACTIVE.

### FI-10: Multiple Simultaneous Faults

**Procedure:**
1. Disconnect encoder AND lower battery voltage simultaneously.
2. **Expected:** System handles both faults, motors stopped.
3. **Verify:** Both fault flags set in diagnostics.

---

## Test Report Template

```
Test ID:      ____
Test Name:    ____
Date:         ____
Tester:       ____
Firmware Ver: ____

Preconditions:
  - [ ] Robot powered
  - [ ] USB connected
  - [ ] Serial monitor open at 115200
  - [ ] MC:READY received

Steps:
  1. ____
  2. ____
  3. ____

Expected Result:
  ____

Actual Result:
  ____

Pass/Fail: ____

Notes:
  ____
```

---

## Pre-Competition Checklist

- [ ] All unit tests pass
- [ ] All integration tests pass
- [ ] All hardware tests pass
- [ ] Endurance test (30 min) passes
- [ ] Full mission simulation passes
- [ ] Fault injection tests documented
- [ ] Battery fully charged
- [ ] All connectors secure
- [ ] Encoder cables strain-relieved
- [ ] Spare USB cable available
- [ ] PID gains verified on competition surface
- [ ] Wheel diameter measured and updated in Config.h
- [ ] Wheel base measured and updated in Config.h
- [ ] Encoder PPR verified and updated in Config.h
- [ ] Voltage divider ratio calibrated
- [ ] Metal detector sensitivity verified
- [ ] All magnets functioning
- [ ] Lift mechanism travel verified
- [ ] Limit switches positioned correctly
- [ ] Buzzer audible at competition distance
- [ ] LED visible in competition lighting
