# Software Architecture

**Minesweeper Robot Motor Controller — Firmware Architecture Document**

---

## Table of Contents

1. [Layered Architecture](#layered-architecture)
2. [HAL Layer](#hal-layer)
3. [Driver Layer](#driver-layer)
4. [Control Layer](#control-layer)
5. [Communication Layer](#communication-layer)
6. [Safety Layer](#safety-layer)
7. [Diagnostics Layer](#diagnostics-layer)
8. [Data Flow](#data-flow)
9. [Timing and Scheduling](#timing-and-scheduling)
10. [Interrupt Service Routines](#interrupt-service-routines)
11. [Task Scheduling](#task-scheduling)
12. [State Machines](#state-machines)

---

## Layered Architecture

The firmware is organized into six layers.  Each layer depends only on the layers below it.  No circular dependencies exist.

```
┌───────────────────────────────────────────────────────┐
│                 APPLICATION LAYER                     │
│         MotorController.ino (setup/loop)              │
├───────────────────────────────────────────────────────┤
│               COMMUNICATION LAYER                     │
│      SerialProtocol (Parser, Telemetry, Errors)       │
├───────────────────────────────────────────────────────┤
│                  CONTROL LAYER                        │
│  MotionController │ PIDController │ LiftController    │
│  (Ramping/Deadband)│(Anti-Windup) │(State Machine)    │
├───────────────────────────────────────────────────────┤
│               ESTIMATION LAYER                        │
│           Odometry (Diff-Drive Kinematics)            │
├───────────────────────────────────────────────────────┤
│                  DRIVER LAYER                         │
│  MotorDriver │ Encoder │  IMU   │ Sensors             │
│  (Cytron HAL)│(ISR/Quad)│(MPU)  │(Prox/Metal/Buzz)   │
├───────────────────────────────────────────────────────┤
│                INFRASTRUCTURE LAYER                   │
│      Safety │ Diagnostics │ Config                    │
│   (WDT/Faults)│(Stats/SRAM) │(Pins/Tuning)           │
└───────────────────────────────────────────────────────┘
```

### Dependency Graph

```
MotorController.ino
  ├── Config.h            (compile-time constants)
  ├── SerialProtocol      (command input, telemetry output)
  ├── MotionController    (velocity ramping, deadband)
  │    └── Config.h
  ├── PIDController       (closed-loop velocity control)
  │    └── Config.h
  ├── MotorDriver         (Cytron MDD10A PWM/DIR)
  │    └── Config.h
  ├── Encoder             (quadrature counting via ISR)
  │    └── Config.h
  ├── Odometry            (differential drive kinematics)
  │    └── Config.h
  ├── IMU                 (MPU6050 complementary filter)
  │    └── Config.h
  ├── LiftController      (lift motor + electromagnets)
  │    └── Config.h
  ├── Sensors             (proximity, metal detect, buzzer, LED)
  │    └── Config.h
  ├── Safety              (watchdog, timeout, faults)
  │    └── Config.h
  └── Diagnostics         (stats, timing, SRAM)
       └── Safety (for SystemState, FaultFlags)
```

---

## HAL Layer

The Hardware Abstraction Layer isolates hardware-specific code from application logic.

### MotorDriver (Cytron MDD10A)

The MDD10A operates in **sign-magnitude mode**:
- **PWM pin**: Duty cycle controls motor speed (0-255).
- **DIR pin**: HIGH = forward, LOW = reverse.

```
 Application: setOutput(+150)
       │
       ▼
 MotorDriver::setOutput(pwm)
       │
       ├── Clamp to [-255, +255]
       ├── Determine direction (sign of pwm)
       ├── Apply inversion (if motor mounted reversed)
       ├── Write DIR pin (HIGH/LOW)
       └── Write PWM pin (analogWrite)
```

The driver handles motor inversion transparently.  The left motor is typically configured as inverted because it is physically mounted opposite to the right motor.

### Encoder (Quadrature)

Encoders use external interrupts on Phase A.  Phase B determines direction.

```
 Phase A Rising Edge (ISR)
       │
       ├── Read Phase B pin
       ├── Determine direction (+1 or -1)
       ├── Increment/decrement pulse_count_ (volatile)
       └── Increment/decrement total_pulse_count_ (volatile)
```

Pulse counts are read atomically using `ATOMIC_BLOCK(ATOMIC_RESTORESTATE)`:

```
 Application: getPulsesAndReset()
       │
       ├── ATOMIC_BLOCK {
       │     pulses = pulse_count_
       │     pulse_count_ = 0
       │   }
       └── return pulses
```

---

## Driver Layer

### IMU (MPU6050)

The MPU6050 driver uses direct I2C register access via the Wire library.  No external library is required.

```
 Startup Sequence:
   1. Wake device (PWR_MGMT_1 = 0x00)
   2. Set gyro range (±250 °/s)
   3. Set accel range (±2 g)
   4. Set DLPF (44 Hz bandwidth)
   5. Calibrate gyro bias (average N samples at rest)

 Per-Cycle Read:
   1. Burst-read 14 bytes (accel XYZ + temp + gyro XYZ)
   2. Convert raw to physical units
   3. Subtract gyro bias
   4. Run complementary filter for roll/pitch
   5. Integrate gyro Z for yaw (drift-prone, but adequate for short runs)
```

### Sensors

| Sensor | Interface | Read Method |
|--------|-----------|-------------|
| Proximity 1-5 | Analog (A1-A5) | `analogRead()` with averaging |
| Metal Detector | Digital (D27) | `digitalRead()` with debounce |
| Buzzer | PWM (D6) | `tone()` / `noTone()` patterns |
| Warning LED | Digital (D13) | Blink patterns via millis() |
| Temperature | (Reserved) | (Not connected) |

---

## Control Layer

### PID Controller

Custom PID implementation with:

```
 error = setpoint - measurement

 P_term = Kp × error
 I_term += Ki × error × dt     (clamped to ±INTEGRAL_WINDUP_LIMIT)
 D_term = Kd × (error - prev_error) / dt

 output = P_term + I_term + D_term

 if output saturates:
     I_term = output_limit - P_term - D_term   (back-calculation anti-windup)
```

### Motion Controller

Handles velocity profiling before the PID:

```
 Target Velocity (from serial command)
       │
       ▼
 Velocity Ramping
   ├── Acceleration limit: MAX_ACCELERATION × dt
   ├── Deceleration limit: MAX_DECELERATION × dt
   └── Output: profiled_velocity
       │
       ▼
 PID Controller
   ├── Setpoint: profiled_velocity
   ├── Input: measured_velocity
   └── Output: PWM command
       │
       ▼
 Deadband Compensation
   ├── If target ≈ 0: output = 0
   ├── If |output| < MIN_DEADBAND: boost to MIN_DEADBAND
   └── Preserves sign
       │
       ▼
 Motor Driver
```

### Lift Controller

State machine for mine pickup mechanism:

```
                 ┌──────────┐
                 │   IDLE   │◄──────────────────┐
                 └────┬─────┘                   │
          LIFT:UP cmd │           LIFT:DN cmd    │
                 ┌────▼─────┐   ┌──────────┐    │
                 │ RAISING  ├──►│  RAISED  │    │
                 └──────────┘   └────┬─────┘    │
                top limit SW        │ LIFT:DN    │
                                ┌───▼──────┐    │
                                │ LOWERING ├────┘
                                └──────────┘
                              bottom limit SW

 FAULT state: entered on stall timeout or limit switch error
```

---

## Communication Layer

### SerialProtocol

The serial protocol is a character-oriented state machine:

```
 State Machine:

 WAITING_PREFIX ──┬── 'r' ──► READING_DIRECTION (right wheel)
                  ├── 'l' ──► READING_DIRECTION (left wheel)
                  ├── 'C' ──► READING_COMMAND (extended command)
                  └── other ── (ignored)

 READING_DIRECTION ──┬── 'p' ──► READING_VALUE (positive)
                     ├── 'n' ──► READING_VALUE (negative)
                     └── other ── WAITING_PREFIX (reset)

 READING_VALUE ──┬── ',' ──► Store value, WAITING_PREFIX
                 ├── '\n' ──► Store value, check completeness
                 ├── digit/dot ── accumulate in buffer
                 └── other ── WAITING_PREFIX (reset)
```

### Telemetry Output

Telemetry is sent using `Serial.print()` with `F()` macro for flash-stored strings:

```
 Velocity:   rp2.310,ln1.280,
 Odometry:   O:0.1234,0.5678,1.2345,
 Metal:      M:1
 Proximity:  P:120,340,560,780,900
 IMU:        I:45.2,-2.1,0.3
 Lift:       L:RAISED
 Diagnostic: D:STATE=1,LOOPS=...
 Status:     S:message
 Error:      E:code:message
```

---

## Safety Layer

### State Machine

```
                    ┌──────────┐
        Power-on ──►│ BOOTING  │
                    └────┬─────┘
                         │ init complete
                    ┌────▼─────┐
          ┌────────►│  ACTIVE  │◄────────┐
          │         └──┬──┬──┬─┘         │
          │            │  │  │           │
          │ clear    timeout  fault   clear
          │         ┌──▼──┐ ┌▼────────┐  │
          │         │ESTOP│ │ FAULT_* │  │
          │         │_TMO │ │(ENC/BAT/│──┘
          └─────────┤     │ │IMU/LIFT)│
                    └─────┘ └─────────┘
```

### Watchdog Timer

The ATmega2560 hardware watchdog is configured with a 2-second timeout.  `wdt_reset()` is called at the top of every `loop()` iteration.  If `loop()` hangs for more than 2 seconds, the MCU resets and `MCUSR.WDRF` is set, which the firmware detects on the next boot.

### Command Timeout

If no valid serial command is received within `COMMAND_TIMEOUT_MS` (1500 ms), the system transitions to `ESTOP_TIMEOUT` and all motors are stopped.  The timeout clears automatically when the next valid command arrives.

---

## Diagnostics Layer

### Runtime Statistics

| Metric | Update Rate | Description |
|--------|-------------|-------------|
| Loop Count | Every iteration | Total loop() iterations |
| Control Cycles | 10 Hz | PID computation cycles |
| Command Count | On receive | Total valid commands received |
| Timeout Count | On timeout | Total timeout events |
| Loop Time (min/max/avg) | Every 100 loops | Microsecond-precision timing |
| Free SRAM | Every status report | Available heap/stack memory |
| Uptime | Every status report | Seconds since boot |

---

## Data Flow

### Command Data Flow (ROS → Motor)

```
 Raspberry Pi (ROS 2)
       │
       │ USB Serial: "rp02.50,ln01.30,"
       ▼
 SerialProtocol::processInput()
       │
       ├── Parse wheel identifier (r/l)
       ├── Parse direction (p/n)
       ├── Parse velocity value (atof)
       ├── Apply sign
       └── Return (right_vel, left_vel)
       │
       ▼
 MotionController::setTarget()
       │
       ├── Velocity sanity check (clamp to MAX_VELOCITY_RAD_S)
       └── Store as target_velocity
       │
       ▼
 MotionController::updateProfiles()
       │
       ├── Apply acceleration/deceleration ramp
       └── Output profiled_velocity
       │
       ▼
 PIDController::compute()
       │
       ├── Calculate error (profiled - measured)
       ├── P + I + D terms
       ├── Anti-windup (integral clamp + back-calculation)
       └── Output PWM value [-255, +255]
       │
       ▼
 MotionController::applyDeadband()
       │
       ├── Zero output if target ≈ 0
       └── Boost small outputs past friction threshold
       │
       ▼
 MotorDriver::setOutput()
       │
       ├── Set DIR pin (HIGH/LOW)
       └── Set PWM pin (analogWrite)
       │
       ▼
 Physical Motor → Wheels → Robot Moves
```

### Sensor Data Flow (Robot → ROS)

```
 Physical Robot
       │
       ├── Encoder pulses ──► ISR ──► pulse_count_ (volatile)
       ├── MPU6050 ──► I2C read ──► IMU::update()
       ├── Metal Detector ──► digitalRead ──► Sensors::update()
       ├── Proximity ──► analogRead ──► Sensors::update()
       └── Limit Switches ──► digitalRead ──► LiftController::update()
       │
       ▼
 Main Loop (processControlLoop)
       │
       ├── Encoder::getPulsesAndReset() [ATOMIC]
       ├── Encoder::calculateVelocity()
       └── Odometry::update()
       │
       ▼
 Main Loop (processTelemetry)
       │
       ├── SerialProtocol::sendTelemetry() → "rp2.31,ln1.28,"
       ├── SerialProtocol::sendOdometry()  → "O:x,y,θ,"
       ├── SerialProtocol::sendIMU()       → "I:yaw,pitch,roll"
       ├── SerialProtocol::sendSensors()   → "M:1" / "P:v1,v2,v3,v4,v5"
       └── SerialProtocol::sendLiftState() → "L:RAISED"
       │
       ▼
 USB Serial
       │
       ▼
 Raspberry Pi (ROS 2) parses telemetry
```

---

## Timing and Scheduling

All timing uses `millis()` for non-blocking scheduling.  **No `delay()` calls exist anywhere.**

### Task Schedule

| Task | Interval | Priority | Method |
|------|----------|----------|--------|
| Serial Receive | Every loop() | Highest | `Serial.available()` polling |
| Watchdog Reset | Every loop() | Highest | `wdt_reset()` |
| Safety Monitor | Every loop() | High | `safetyMonitor.update()` |
| Motor Control | 100 ms (10 Hz) | High | `processControlLoop()` |
| IMU Read | 20 ms (50 Hz) | Medium | `imu.update()` |
| Sensor Read | 50 ms (20 Hz) | Medium | `sensors.update()` |
| Lift Update | 50 ms (20 Hz) | Medium | `liftController.update()` |
| Telemetry TX | 100 ms (10 Hz) | Normal | `processTelemetry()` |
| Diagnostics TX | 5000 ms (0.2 Hz) | Low | `diagnostics.sendStatusReport()` |

### Timing Diagram (one loop iteration)

```
 Time ──────────────────────────────────────────────────►
 │
 ├─ loopStart() ─────── ~2 µs
 ├─ wdt_reset() ─────── ~1 µs
 ├─ processInput() ──── ~20 µs (if data available)
 ├─ [if control tick]:
 │   ├─ getPulsesAndReset() ── ~5 µs
 │   ├─ calculateVelocity() ── ~10 µs
 │   ├─ updateProfiles() ───── ~5 µs
 │   ├─ PID compute × 2 ────── ~20 µs
 │   ├─ applyDeadband() ─────── ~3 µs
 │   ├─ setOutput() × 2 ────── ~5 µs
 │   └─ odometry.update() ──── ~15 µs
 ├─ [if telemetry tick]:
 │   └─ sendTelemetry() ────── ~50 µs
 ├─ [if IMU tick]:
 │   └─ imu.update() ──────── ~200 µs (I2C)
 ├─ [if sensor tick]:
 │   └─ sensors.update() ──── ~100 µs
 ├─ safety.update() ──── ~10 µs
 ├─ diagnostics.send() ── ~0 µs (unless 5s tick)
 └─ loopEnd() ─────────── ~2 µs

 Typical total: 50-350 µs per iteration
 Worst-case: ~500 µs (all tasks fire simultaneously)
```

---

## Interrupt Service Routines

### Encoder ISRs

Two ISRs are registered, one for each encoder's Phase A pin:

| ISR | Trigger | Pin | Action |
|-----|---------|-----|--------|
| `rightEncoderISR()` | RISING edge | D3 (INT5) | Read Phase B, update count |
| `leftEncoderISR()` | RISING edge | D2 (INT4) | Read Phase B, update count |

**ISR design rules:**
- No `digitalRead()` (uses direct port register access via `digitalRead` — acceptable on Mega at current speeds, would use PINx for optimization if needed)
- No floating-point operations
- No Serial output
- No memory allocation
- Variables shared with main code are `volatile`
- Multi-byte shared reads use `ATOMIC_BLOCK`
- Execution time: < 2 µs per interrupt

---

## Task Scheduling

The firmware uses a **cooperative non-preemptive scheduler** built on `millis()` comparisons:

```cpp
// In loop():
unsigned long now = millis();

// 10 Hz control loop
if (now - last_control_time >= CONTROL_INTERVAL_MS) {
    last_control_time = now;
    processControlLoop();
}

// 50 Hz IMU
if (now - last_imu_time >= IMU_INTERVAL_MS) {
    last_imu_time = now;
    imu.update();
}

// 20 Hz sensors
if (now - last_sensor_time >= SENSOR_INTERVAL_MS) {
    last_sensor_time = now;
    sensors.update();
}
```

This approach:
- **Avoids `delay()`** (fully non-blocking)
- **Is deterministic** (each task runs at its configured rate)
- **Is simple** (no RTOS overhead)
- **Handles jitter gracefully** (tasks that miss their deadline simply run on the next iteration)

---

## State Machines

### System State Machine (Safety)

See [Safety.md](Safety.md) for full documentation.

| State | Entry Condition | Exit Condition |
|-------|----------------|----------------|
| BOOTING | Power-on / WDT reset | Init complete |
| ACTIVE | Init complete / fault cleared | Timeout / fault / E-Stop |
| ESTOP_TIMEOUT | No command for 1500 ms | Next valid command |
| ESTOP_MANUAL | Manual E-Stop command | Clear command |
| FAULT_ENCODER | Encoder error detected | Error cleared |
| FAULT_BATTERY | Voltage out of range | Voltage returns to range |
| FAULT_IMU | MPU6050 communication failure | I2C restored |
| FAULT_LIFT | Lift motor stall / limit error | Fault cleared |

### Lift State Machine

| State | Entry Condition | Exit Condition |
|-------|----------------|----------------|
| IDLE | Power-on / lower complete | LIFT:UP command |
| RAISING | LIFT:UP command | Top limit switch |
| RAISED | Top limit switch reached | LIFT:DN command |
| LOWERING | LIFT:DN command | Bottom limit switch |
| LOWERED | Bottom limit switch reached | (transitions to IDLE) |
| FAULT | Stall timeout / switch error | Manual reset |

### Buzzer Pattern Machine

| Pattern | Meaning | Sequence |
|---------|---------|----------|
| SILENT | Normal operation | Off |
| BEEP | Command received acknowledgment | 50 ms on |
| ALERT | Warning condition | 200 ms on, 200 ms off |
| ALARM | Fault condition | 100 ms on, 100 ms off (fast) |
| MINE_DETECT | Metal detector triggered | 500 Hz continuous |
