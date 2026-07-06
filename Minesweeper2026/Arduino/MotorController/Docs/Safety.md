# Safety Systems

**Minesweeper Robot Motor Controller — Safety Documentation**

---

## Safety Philosophy

The safety system follows a **fail-safe** design philosophy:

1. **If in doubt, stop.** Any ambiguous condition results in motor shutdown.
2. **Defense in depth.** Multiple independent safety layers protect against different failure modes.
3. **No single point of failure.** The watchdog timer provides an independent hardware safety net even if software hangs.
4. **Transparent diagnostics.** All fault conditions are reported via serial for remote monitoring.

---

## Safety Layers

```
┌─────────────────────────────────────────────┐
│ Layer 5: Hardware Watchdog (ATmega2560 WDT) │  ← Independent hardware timer
├─────────────────────────────────────────────┤
│ Layer 4: Command Timeout Detection          │  ← Serial link health
├─────────────────────────────────────────────┤
│ Layer 3: Fault Detection (Encoder/IMU/Lift) │  ← Sensor health
├─────────────────────────────────────────────┤
│ Layer 2: Battery Voltage Monitoring         │  ← Power health
├─────────────────────────────────────────────┤
│ Layer 1: Velocity Sanity Limits             │  ← Command validation
├─────────────────────────────────────────────┤
│ Layer 0: Motor Driver Hardware Limits       │  ← PWM clamping
└─────────────────────────────────────────────┘
```

---

## Emergency Stop (E-Stop)

### Behavior

When E-Stop is triggered (any source):
1. Both drive motor PWM outputs are set to 0 immediately.
2. Lift motor is stopped.
3. PID controllers are disabled and integral terms reset.
4. Target velocities are set to 0.
5. System state transitions to the appropriate E-Stop state.
6. Serial diagnostic message is sent.

### Trigger Sources

| Source | State | Recovery |
|--------|-------|----------|
| Command timeout | `ESTOP_TIMEOUT` | Next valid velocity command |
| Manual serial command (`CESTOP`) | `ESTOP_MANUAL` | `CCLEAR` serial command |
| Encoder fault | `FAULT_ENCODER` | Fault condition clears |
| Battery fault | `FAULT_BATTERY` | Voltage returns to safe range |
| IMU fault | `FAULT_IMU` | I2C communication restored |
| Lift fault | `FAULT_LIFT` | Manual fault clear command |

### Recovery

The `clearEStop()` method handles recovery from both timeout and manual E-Stop:

```
ESTOP_TIMEOUT + valid command ──► ACTIVE
ESTOP_MANUAL  + CCLEAR command (after 100ms debounce) ──► ACTIVE
FAULT_*       + fault condition clears ──► ACTIVE
```

---

## Hardware Watchdog

### Configuration

| Parameter | Value |
|-----------|-------|
| Timer | ATmega2560 Watchdog Timer (WDT) |
| Timeout | 2 seconds (`WDTO_2S`) |
| Reset Action | Full MCU reset |
| Feature Flag | `ENABLE_WATCHDOG` in Config.h |

### Operation

1. **Initialization**: `wdt_enable(WDTO_2S)` in `SafetyMonitor::begin()`.
2. **Reset**: `wdt_reset()` called at the top of every `loop()` iteration.
3. **Trigger**: If `loop()` hangs for > 2 seconds, the WDT resets the MCU.
4. **Detection**: On boot, `MCUSR.WDRF` is checked. If set, a watchdog reset occurred and `faults_.watchdog_reset` is flagged.

### What Triggers a Watchdog Reset

| Scenario | Duration | Result |
|----------|----------|--------|
| Infinite loop in user code | > 2 seconds | WDT reset |
| I2C bus lockup (MPU6050) | > 2 seconds | WDT reset |
| Stack overflow | Immediate | WDT reset (or crash) |
| Normal operation | < 1 ms per loop | WDT reset never triggers |

### Post-Reset Behavior

After a WDT reset:
1. `MCUSR.WDRF` flag is detected and logged.
2. System boots normally into `ACTIVE` state.
3. Diagnostics report includes `watchdog_reset = true`.
4. Motors remain stopped until first valid command.

---

## Command Timeout

### Configuration

| Parameter | Value |
|-----------|-------|
| Timeout | 1500 ms (`COMMAND_TIMEOUT_MS`) |
| Source | `SerialProtocol::getLastCommandTime()` |
| Check Rate | Every `loop()` iteration |

### Behavior

```
 Time ──────────────────────────────────────►
 │
 ├── Command received ──► lastCommandTime = millis()
 │                        State: ACTIVE
 │
 ├── 500 ms ── no command
 │
 ├── 1000 ms ── no command
 │
 ├── 1500 ms ── TIMEOUT TRIGGERED
 │              ├── Motors stopped
 │              ├── State → ESTOP_TIMEOUT
 │              └── "SAFETY: Command timeout" sent
 │
 ├── Command received ──► State → ACTIVE (auto-recovery)
 │
```

### Why 1500 ms?

- ROS 2 typically sends commands at 10 Hz (every 100 ms).
- 1500 ms allows for ~15 missed packets before timeout.
- Short enough to stop before the robot travels too far.
- Long enough to tolerate brief USB communication delays.

---

## Motor Failure Detection

### Stall Detection (Lift)

The lift controller monitors motor travel time:

| Check | Threshold | Action |
|-------|-----------|--------|
| Raise duration | > 5000 ms (`STALL_TIMEOUT_MS`) | Stop motor, enter FAULT |
| Lower duration | > 5000 ms | Stop motor, enter FAULT |

If the lift motor runs for longer than the stall timeout without hitting a limit switch, it is assumed to be stalled or disconnected.

### Drive Motor Monitoring

Current firmware monitors:
- **Velocity sanity**: If measured velocity exceeds 150% of `MAX_VELOCITY_RAD_S`, a fault flag is set.
- **Command-measurement mismatch**: (Future) If PID output is saturated but measured velocity remains zero, motor or encoder may be disconnected.

---

## Encoder Failure Detection

### Detection Methods

| Method | Implementation |
|--------|---------------|
| Zero velocity with non-zero command | Future: monitor PID saturation |
| Error flag from encoder class | `encoder.hasError()` |
| Manual fault setting | `safetyMonitor.setEncoderFault(side, true)` |

### Response

When an encoder fault is detected:
1. System transitions to `FAULT_ENCODER`.
2. Motors are stopped.
3. Error message sent via serial.

---

## Sensor Failure Detection

### IMU (MPU6050)

| Check | Method | Frequency |
|-------|--------|-----------|
| I2C presence | WHO_AM_I register read | On boot |
| Communication timeout | Wire read timeout | Every update |
| Data validity | NaN / extreme value check | Every update |

If the IMU fails:
1. `imu.hasError()` returns true.
2. `SafetyMonitor` sets `FAULT_IMU`.
3. Odometry continues using wheel encoders only (degraded mode).
4. Error message sent.

### Metal Detector

- Debounced with 100 ms window to avoid false triggers.
- No fault detection (passive sensor).

### Proximity Sensors

- ADC values checked for rail conditions (stuck at 0 or 1023).
- No active fault response (informational only).

---

## Battery Failure Detection

### Voltage Monitoring

| Parameter | Value | Action |
|-----------|-------|--------|
| Low threshold | 10.8 V (10800 mV) | `FAULT_BATTERY`, motors stop |
| High threshold | 16.8 V (16800 mV) | `FAULT_BATTERY`, motors stop |
| ADC pin | A0 | Voltage divider with ratio 3.3:1 |
| Read rate | Every safety update (per loop) | `analogRead(A0)` |

### Voltage Calculation

```
ADC value (0-1023) → ADC millivolts = ADC × 5000 / 1024
Battery millivolts = ADC_mV × VOLTAGE_DIVIDER_RATIO
```

### Response

- Low voltage: Motors stopped to prevent brown-out.
- High voltage: Motors stopped to prevent over-voltage damage.
- Battery fault clears automatically when voltage returns to safe range.

---

## Safe Startup Sequence

```
 1. Power applied to Arduino
 2. setup() called
 3. Check MCUSR for watchdog reset flag
 4. Disable watchdog temporarily
 5. Initialize all GPIO pins to safe states (outputs LOW)
 6. Initialize motor drivers (PWM = 0, DIR = HIGH)
 7. Initialize encoders (attach interrupts)
 8. Initialize PID controllers
 9. Initialize serial communication
10. Initialize safety monitor
11. Enable watchdog (2-second timeout)
12. Initialize IMU (calibrate gyro ~2.5 seconds)
13. Initialize sensors
14. Initialize lift controller
15. Send "MC:READY" on serial
16. Enter main loop
17. Motors remain stopped until first valid command received
```

**Key safety invariants during startup:**
- Motors are never powered before safety systems are initialized.
- Watchdog is enabled before IMU calibration (which blocks for ~2.5s, but WDT is reset within calibration loop).
- No motor output until a valid velocity command arrives from ROS.

---

## Safe Shutdown

There is no explicit shutdown command.  Power removal is the shutdown mechanism.

**Before power removal:**
1. Send `rp00.00,lp00.00,` to stop motors.
2. Send `CMAG:ALL:OFF\n` to de-energize electromagnets.
3. Wait 500 ms for commands to take effect.
4. Remove power.

**If power is removed unexpectedly:**
- Motors coast to a stop (no dynamic braking in sign-magnitude mode).
- Electromagnets release (normally de-energized).
- Next power-on follows safe startup sequence.

---

## System State Machine

```
                          ┌──────────────┐
          Power-on/Reset──►   BOOTING    │
                          └──────┬───────┘
                                 │ init complete
                          ┌──────▼───────┐
              ┌──────────►│    ACTIVE    │◄──────────────┐
              │           └──┬──┬──┬──┬──┘               │
              │              │  │  │  │                  │
              │           timeout │ enc  batt           clear
              │              │  │ fault fault            │
              │  ┌───────────▼┐│┌▼─────┐ ┌▼─────┐  ┌───┴──────┐
              │  │  ESTOP     │││FAULT │ │FAULT │  │  ESTOP   │
              │  │  TIMEOUT   │││ENCODE│ │BATT  │  │  MANUAL  │
              │  └──────┬─────┘│└──────┘ └──────┘  └──────────┘
              │         │      │
              │  valid  │   imu fault / lift fault
              │  command│      │
              │         │  ┌───▼──────┐
              └─────────┘  │ FAULT_*  │
                           │(IMU/LIFT)│
                           └──────────┘
```

### State Transition Table

| From | Event | To | Action |
|------|-------|----|--------|
| BOOTING | Init complete | ACTIVE | Enable watchdog |
| ACTIVE | Command timeout | ESTOP_TIMEOUT | Stop motors |
| ACTIVE | Manual E-Stop | ESTOP_MANUAL | Stop motors |
| ACTIVE | Encoder fault | FAULT_ENCODER | Stop motors |
| ACTIVE | Battery fault | FAULT_BATTERY | Stop motors |
| ACTIVE | IMU fault | FAULT_IMU | Stop motors (degraded) |
| ACTIVE | Lift fault | FAULT_LIFT | Stop lift motor |
| ESTOP_TIMEOUT | Valid command | ACTIVE | Resume operation |
| ESTOP_MANUAL | Clear command | ACTIVE | Resume operation |
| FAULT_ENCODER | Fault clears | ACTIVE | Resume operation |
| FAULT_BATTERY | Voltage OK | ACTIVE | Resume operation |
| FAULT_IMU | I2C restored | ACTIVE | Resume operation |
| FAULT_LIFT | Manual clear | ACTIVE | Resume operation |
