# Motion Control

**Minesweeper Robot — Motion Control Theory and Tuning Guide**

---

## Differential Drive Model

The robot uses a **4WD differential drive** configuration where both wheels on each side are mechanically coupled.  The kinematic model treats it as a two-wheel differential drive.

### Unicycle Model

```
     ┌────────────────────────┐
     │         Robot          │
     │                        │
     │  Left     ●────●  Right│
     │  Wheel   [Body]  Wheel│
     │           │            │
     │           ▼ θ (heading)│
     └────────────────────────┘

     v_robot = (v_right + v_left) / 2       Linear velocity
     ω_robot = (v_right - v_left) / L       Angular velocity

     where:
       v_right = ω_right × r    (right wheel linear velocity, m/s)
       v_left  = ω_left  × r    (left wheel linear velocity, m/s)
       r = wheel radius (0.0325 m)
       L = wheel base (0.150 m)
```

### Forward Kinematics

Given wheel angular velocities (rad/s), compute robot velocity:

```
v_right_linear = ω_right × WHEEL_RADIUS_M
v_left_linear  = ω_left  × WHEEL_RADIUS_M

v = (v_right_linear + v_left_linear) / 2
ω = (v_right_linear - v_left_linear) / WHEEL_BASE_M
```

### Inverse Kinematics

Given desired robot velocity (v, ω), compute wheel speeds:

```
v_right = v + ω × L/2
v_left  = v - ω × L/2

ω_right = v_right / r
ω_left  = v_left  / r
```

> **Note:** The ROS 2 layer handles inverse kinematics.  The Arduino receives individual wheel velocities in rad/s.

---

## PID Controller

### Architecture

Each drive wheel has an independent PID controller:

```
 Setpoint (profiled_velocity)
       │
       ▼
     ┌─┤ Error = setpoint - measured
     │ │
     │ ├── P_term = Kp × error
     │ │
     │ ├── I_term += Ki × error × dt
     │ │     └── Clamped to ±INTEGRAL_WINDUP_LIMIT
     │ │
     │ ├── D_term = Kd × (error - prev_error) / dt
     │ │
     │ └── output = P + I + D
     │       └── Clamped to [OUTPUT_MIN, OUTPUT_MAX]
     │             └── If saturated: back-calculate I_term
     │
     └──► PWM output → Motor Driver
```

### Tuning Parameters

| Parameter | Right Motor | Left Motor | Units |
|-----------|-------------|------------|-------|
| Kp | 11.5 | 12.8 | PWM/(rad/s) |
| Ki | 7.5 | 8.3 | PWM/(rad·s) |
| Kd | 0.1 | 0.1 | PWM·s/rad |
| Output Min | -255 | -255 | PWM |
| Output Max | +255 | +255 | PWM |
| Integral Limit | ±200 | ±200 | PWM |
| Sample Time | 100 | 100 | ms |

### Why Different Gains?

Left and right motors may have slightly different:
- Friction characteristics
- Gear ratios (manufacturing tolerance)
- Wire resistance
- Encoder alignment

Independent tuning compensates for these asymmetries.

### Anti-Windup

Two anti-windup mechanisms work together:

1. **Integral clamping**: The integral term is limited to `±INTEGRAL_WINDUP_LIMIT` (±200).

2. **Back-calculation**: When the total PID output saturates at `OUTPUT_MAX` or `OUTPUT_MIN`, the integral term is back-calculated:
   ```
   I_term = output_limit - P_term - D_term
   ```
   This prevents the integral term from growing when the output is already at its limit.

### Tuning Procedure

1. **Start with Ki = 0, Kd = 0.**
2. **Increase Kp** until the motor responds briskly to step inputs without excessive oscillation.  Start at Kp ≈ 10.
3. **Add Ki** to eliminate steady-state error.  Start at Ki ≈ 5.  Increase until the motor reaches setpoint within ~300 ms.
4. **Add Kd** to reduce overshoot.  Start at Kd ≈ 0.1.  Keep small to avoid amplifying encoder noise.
5. **Test at different velocities** (0.5, 1.0, 2.0, 5.0 rad/s) and under load.
6. **Verify anti-windup** by commanding velocity, stalling the wheel, then releasing.

---

## Velocity Ramping

### Purpose

Instant velocity changes cause:
- High current spikes (stress on motor driver and battery)
- Wheel slip on low-traction surfaces
- Mechanical shock to gearboxes
- PID controller instability

### Implementation

The Motion Controller applies acceleration/deceleration limits between the commanded velocity and the PID setpoint:

```
 target_velocity (from serial command)
       │
       ├── Δv = target - current_profiled
       │
       ├── If accelerating:
       │     max_step = MAX_ACCELERATION × dt
       │     (15.0 rad/s² × 0.1s = 1.5 rad/s per step)
       │
       ├── If decelerating:
       │     max_step = MAX_DECELERATION × dt
       │     (20.0 rad/s² × 0.1s = 2.0 rad/s per step)
       │
       ├── If |Δv| ≤ max_step:
       │     profiled = target (reached target)
       │
       └── Else:
             profiled += sign(Δv) × max_step
```

### Configuration

| Parameter | Value | Effect |
|-----------|-------|--------|
| MAX_ACCELERATION | 15.0 rad/s² | ~1.5 rad/s per 100ms step |
| MAX_DECELERATION | 20.0 rad/s² | ~2.0 rad/s per 100ms step |

Deceleration is faster than acceleration for safety (quicker stops).

### Timing Example

Accelerating from 0 to 5 rad/s:
```
 t=0.0s: profiled = 0.0
 t=0.1s: profiled = 1.5
 t=0.2s: profiled = 3.0
 t=0.3s: profiled = 4.5
 t=0.4s: profiled = 5.0 (reached target)
```

Total ramp time: 400 ms.

---

## Heading Control (IMU Correction)

### Current Implementation

The current firmware does **not** close a heading loop.  Each wheel runs an independent velocity PID.

### Future Enhancement: Heading PID

With the MPU6050 providing yaw data, a heading PID can correct for differential wheel slip:

```
 heading_error = desired_heading - imu.getYaw()
 heading_correction = heading_kp × heading_error

 right_target += heading_correction
 left_target  -= heading_correction
```

This corrects for:
- Unequal tire pressure
- Surface friction asymmetry
- Mechanical misalignment
- Accumulated odometry drift

---

## Wheel Slip Detection

### Theory

In normal operation, the differential-drive yaw rate should match the IMU yaw rate:

```
ω_odom = (v_right - v_left) / L    (from encoders)
ω_imu  = gyro_z                     (from MPU6050)

slip_indicator = |ω_odom - ω_imu|
```

If `slip_indicator > threshold`, one or both wheels are slipping.

### Response (Future)

- Reduce commanded velocity
- Flag in diagnostics
- Alert ROS 2 for path replanning

---

## Deadband Compensation

### Problem

At very low PWM values, the motor torque is insufficient to overcome static friction (stiction).  The motor buzzes but doesn't rotate.

### Solution

```
if target_velocity ≈ 0:
    output = 0                    // True stop, no buzzing

else if |pid_output| < MIN_OUTPUT_DEADBAND:
    output = sign(pid_output) × MIN_OUTPUT_DEADBAND   // Boost past stiction
```

| Parameter | Value | Effect |
|-----------|-------|--------|
| MIN_OUTPUT_DEADBAND | 8 PWM | Minimum PWM to overcome stiction |
| VELOCITY_TOLERANCE | 0.01 rad/s | Threshold for "near zero" |

### Calibration

To find the correct deadband value:
1. Slowly increase PWM from 0 until the wheel just starts turning.
2. Note this PWM value.
3. Set `MIN_OUTPUT_DEADBAND` to this value (or slightly above).

---

## Odometry

### Position Integration

The odometry module integrates wheel velocities to estimate robot pose (x, y, θ).

#### Straight-Line Motion (ω ≈ 0)

```
x += v × dt × cos(θ)
y += v × dt × sin(θ)
```

#### Arc Motion (ω ≠ 0)

```
R = v / ω                              (turning radius)
x += R × (sin(θ + ω×dt) - sin(θ))
y += R × (-cos(θ + ω×dt) + cos(θ))
θ += ω × dt
```

The arc integration is more accurate than Euler integration for curved paths.

### Heading Normalization

The heading θ is normalized to [-π, π] after every update to prevent floating-point overflow during long runs.

### Accuracy Limitations

| Source | Impact | Mitigation |
|--------|--------|------------|
| Encoder resolution | ±0.016 rad (1 pulse) | Use high-PPR encoders |
| Wheel diameter tolerance | ±1% position error/meter | Measure and calibrate |
| Wheel base tolerance | ±2° heading error/meter | Measure and calibrate |
| Wheel slip | Unbounded drift | IMU heading correction |
| Integration drift | ~2-5% per meter | Sensor fusion with vision |

> **Critical:** Odometry alone is insufficient for reliable localization. The ROS 2 layer should fuse odometry with IMU, vision, and potentially SLAM for accurate position estimation.

---

## Encoder Calculations

### Velocity from Pulses

```
pulses = getPulsesAndReset()         // Signed pulse count over dt
rpm = (pulses / PPR) × (60 / dt)    // Revolutions per minute
ω = rpm × (2π / 60)                 // Angular velocity in rad/s
```

Simplified:
```
ω = pulses × (2π) / (PPR × dt)     // rad/s
```

### Linear Velocity

```
v = ω × WHEEL_RADIUS_M              // m/s
```

### Distance from Pulses

```
distance = total_pulses × (2π × WHEEL_RADIUS_M / PPR)   // meters
```

---

## Control Loop Timing

### Loop Architecture

```
 Time ──────────────────────────────────────────►
 │
 0ms        100ms       200ms       300ms
 │           │           │           │
 ├─ Control ─├─ Control ─├─ Control ─├─ Control
 ├─ Telem   ─├─ Telem   ─├─ Telem   ─├─ Telem
 ├─ IMU ────────┤ IMU ────────┤ IMU ────────┤
 │  20ms        │ 40ms        │ 60ms        │
 ├─ Sensor ──────────┤ Sensor ──────────┤ Sensor
 │  50ms              │ 100ms             │ 150ms
 │
 Safety + Serial: Every iteration (~50µs)
```

### Jitter Analysis

| Source | Typical | Worst Case |
|--------|---------|------------|
| `analogRead()` | 112 µs | 120 µs |
| I2C read (14 bytes) | 200 µs | 500 µs |
| `Serial.print()` | 50 µs | 100 µs |
| PID compute | 15 µs | 20 µs |
| Total control tick | 300 µs | 800 µs |

The 100 ms control interval provides ample margin.  Even worst-case, the loop completes in < 1 ms, leaving 99 ms of slack.

### Critical Timing Requirements

| Requirement | Target | Actual |
|-------------|--------|--------|
| Control loop determinism | ±5% jitter | < 1% (millis-based) |
| ISR latency | < 5 µs | < 2 µs |
| Serial response time | < 200 ms | ~100 ms |
| E-Stop response | < 100 ms | < 10 ms (next loop) |
| Watchdog reset margin | > 50% | > 99% (2s timeout, <1ms loop) |
