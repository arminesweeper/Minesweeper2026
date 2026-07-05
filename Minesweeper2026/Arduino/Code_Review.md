# Minesweeper Robot Motor Controller Code Review

## Executive Summary

**Overall score:** 7.5/10

The controller demonstrates a solid understanding of closed-loop motor control using encoder feedback and independent PID loops for each motor. The overall structure is readable and suitable for a university or prototype robotics project. However, several embedded-systems best practices are missing that affect robustness, long-term reliability, and safety.

---

# Strengths

## Architecture

- Clear separation between serial command parsing and PID control.
- Independent control loops for left and right motors.
- Encoder interrupts are used instead of polling.
- Code is easy to follow.

## Motor Control

- Closed-loop velocity control.
- Separate PID tuning for each motor.
- Direction handled independently from PWM.

## Communication

- Lightweight serial protocol.
- Reasonable baud rate (115200).

---

# Issues and Recommendations

## Critical

### 1. Avoid `String`

Using `String` on AVR causes heap fragmentation.

Current:

```cpp
String encoder_read;
String right_wheel_sign;
```

Recommendation:

- Use `char` or `int8_t` for direction.
- Use `Serial.print()` instead of building `String`s.

Priority: Critical

---

### 2. Shared variables must be `volatile`

Encoder counters are modified inside interrupts.

Current:

```cpp
unsigned int right_encoder_counter;
```

Should become:

```cpp
volatile uint16_t right_encoder_counter;
```

Priority: Critical

---

### 3. Atomic access

Protect encoder reads:

```cpp
noInterrupts();
count = right_encoder_counter;
right_encoder_counter = 0;
interrupts();
```

Otherwise counts may be lost.

Priority: Critical

---

### 4. ISR should be short

Avoid:

```cpp
digitalRead(...)
String assignment
```

inside interrupts.

Prefer direct port access or simple flag updates.

Priority: High

---

### 5. Missing command timeout

If the host crashes the robot continues moving.

Recommended:

- Store `lastCommandTime`
- Stop motors after ~500 ms without commands.

Priority: Critical

---

### 6. PID output limits

Use:

```cpp
SetOutputLimits(0,255);
```

Prevents invalid PWM values and reduces windup.

Priority: High

---

### 7. PID sample time

Explicitly configure:

```cpp
SetSampleTime(100);
```

Priority: High

---

### 8. Anti-windup

Prevent integral accumulation while stalled or saturated.

Priority: High

---

### 9. Velocity ramp

Instant velocity changes increase current and wheel slip.

Add acceleration limiting.

Priority: High

---

### 10. Remove magic numbers

Replace:

- 385
- 60
- 0.10472
- 100

with named constants.

Priority: Medium

---

### 11. Serial parser

Current parser is fragile.

Prefer framed packets.

Example:

```
<R,+,12.35>
<L,-,10.10>
```

Priority: Medium

---

### 12. PWM frequency

Verify timers used by PWM pins and configure frequency if necessary.

Priority: Medium

---

### 13. Deadband compensation

Small PWM values cannot overcome static friction.

Implement minimum PWM threshold.

Priority: Medium

---

### 14. Encoder decoding

Current implementation samples only phase B on phase A interrupt.

Full quadrature decoding improves robustness.

Priority: Medium

---

### 15. Safety

Recommended additions:

- Watchdog
- Battery monitor
- Stall detection
- Encoder fault detection
- Emergency stop
- Brown-out handling

Priority: High

---

# Memory

Avoid dynamic allocation.

Use:

- const
- constexpr
- fixed-size arrays

---

# Real-Time

Good:

- Interrupt-driven encoders
- Periodic PID update

Needs improvement:

- Atomic access
- Faster ISR
- No heap allocation

---

# Suggested Architecture

```
loop()
 ├── ReadSerial()
 ├── ParseCommands()
 ├── SafetyMonitor()
 ├── ReadEncoders()
 ├── ComputeVelocity()
 ├── VelocityRamp()
 ├── ComputePID()
 ├── ApplyMotorOutputs()
 └── PublishDiagnostics()
```

---

# Competition Features

Recommended additions:

- Odometry
- Position estimation
- Heading PID
- Motion profiles
- IMU support
- Battery diagnostics
- Fault logging

---

# Scores

| Category              | Score |
| --------------------- | ----: |
| Readability           |  9/10 |
| Embedded C++          |  6/10 |
| Interrupt Safety      |  5/10 |
| PID                   |  8/10 |
| Communication         |  7/10 |
| Memory                |  5/10 |
| Reliability           |  7/10 |
| Competition Readiness |  6/10 |
| Production Readiness  |  5/10 |

Overall: **7.5/10**

---

# Priority Checklist

- [ ] Remove all `String`
- [ ] Make shared ISR variables `volatile`
- [ ] Read/reset counters atomically
- [ ] Configure PID sample time
- [ ] Configure PID output limits
- [ ] Add command timeout
- [ ] Add acceleration limiting
- [ ] Add anti-windup
- [ ] Replace magic numbers
- [ ] Improve serial protocol
- [ ] Reduce ISR workload
- [ ] Add watchdog
- [ ] Add battery monitoring
- [ ] Add diagnostics
