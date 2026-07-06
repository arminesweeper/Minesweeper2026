# Communication Protocol

**Minesweeper Robot — Serial Communication Specification**

---

## Overview

| Parameter | Value |
|-----------|-------|
| Interface | USB Serial (CDC ACM) |
| Baud Rate | 115200 |
| Data Bits | 8 |
| Stop Bits | 1 |
| Parity | None |
| Flow Control | None |
| Line Ending | `\n` (LF) or `,` (comma delimiter) |
| Max Packet Length | 64 bytes |

---

## Protocol Design Principles

1. **Human-readable** — All packets are ASCII text for easy debugging.
2. **Backward-compatible** — The original `rp02.50,ln01.30,` format is preserved exactly.
3. **Prefix-based routing** — Each packet type has a unique first character.
4. **No dynamic allocation** — Fixed-size buffers throughout.
5. **Robust parsing** — State machine parser with overflow protection and timeout.
6. **Fire-and-forget** — No acknowledgment handshake (simplicity over reliability).

---

## Inbound Packets (ROS → Arduino)

### Velocity Command

The primary command format.  Fully backward-compatible with existing ROS 2 code.

```
rp02.50,ln01.30,
```

#### Field Breakdown

| Position | Field | Values | Description |
|----------|-------|--------|-------------|
| 1 | Wheel | `r` / `l` | Right or left wheel |
| 2 | Direction | `p` / `n` | Positive (forward) or negative (reverse) |
| 3-7 | Value | `XX.XX` | Velocity magnitude in rad/s |
| 8 | Separator | `,` | Comma delimiter |

#### Rules

- Both wheels must be specified in one packet (`r...l...` order).
- The comma after each value is mandatory.
- Maximum value string length: 7 characters (e.g., `99.999`).
- Values are parsed with `atof()`.
- Invalid characters reset the parser to WAITING state.
- A newline (`\n`) may optionally follow the final comma.

#### Examples

| Command | Right Wheel | Left Wheel |
|---------|-------------|------------|
| `rp02.50,lp01.30,` | +2.50 rad/s forward | +1.30 rad/s forward |
| `rn01.00,ln01.00,` | -1.00 rad/s reverse | -1.00 rad/s reverse |
| `rp00.00,lp00.00,` | Stop right | Stop left |
| `rp05.00,ln02.00,` | +5.00 forward | -2.00 reverse (spin) |

### Extended Commands

New command format for lift, magnets, buzzer, and system control.  Prefixed with `C` to distinguish from velocity commands.

```
C<COMMAND>\n
```

#### Lift Commands

| Command | Action |
|---------|--------|
| `CLIFT:UP\n` | Raise lift mechanism |
| `CLIFT:DN\n` | Lower lift mechanism |
| `CLIFT:STOP\n` | Stop lift motor |

#### Magnet Commands

| Command | Action |
|---------|--------|
| `CMAG:1:ON\n` | Energize magnet 1 |
| `CMAG:1:OFF\n` | De-energize magnet 1 |
| `CMAG:2:ON\n` | Energize magnet 2 |
| `CMAG:ALL:ON\n` | Energize all 5 magnets |
| `CMAG:ALL:OFF\n` | De-energize all magnets |

#### Buzzer Commands

| Command | Action |
|---------|--------|
| `CBUZZ:SILENT\n` | Turn off buzzer |
| `CBUZZ:BEEP\n` | Single beep |
| `CBUZZ:ALERT\n` | Alert pattern |
| `CBUZZ:ALARM\n` | Alarm pattern |

#### System Commands

| Command | Action |
|---------|--------|
| `CRESET\n` | Reset all subsystems to default |
| `CDIAG\n` | Request immediate diagnostics report |
| `CESTOP\n` | Trigger emergency stop |
| `CCLEAR\n` | Clear emergency stop |

---

## Outbound Packets (Arduino → ROS)

### Velocity Telemetry

Primary telemetry.  Same format as inbound commands for symmetry.

```
rp2.310,ln1.280,
```

| Field | Description |
|-------|-------------|
| `r`/`l` | Wheel identifier |
| `p`/`n` | Measured direction |
| `X.XXX` | Measured velocity in rad/s (3 decimal places) |
| `,` | Separator |

**Rate:** 10 Hz (every 100 ms)

### Odometry

```
O:0.1234,0.5678,1.2345,
```

| Field | Unit | Description |
|-------|------|-------------|
| First | meters | X position |
| Second | meters | Y position |
| Third | radians | Heading (θ) |

**Rate:** 10 Hz

### Metal Detector

```
M:1
```

| Value | Meaning |
|-------|---------|
| `0` | No metal detected |
| `1` | Metal detected |

**Rate:** On change, or 20 Hz continuous

### Proximity Sensors

```
P:120,340,560,780,900
```

Five comma-separated ADC values (0-1023), one per sensor.

**Rate:** 20 Hz

### IMU Data

```
I:45.2,-2.1,0.3
```

| Field | Unit | Description |
|-------|------|-------------|
| First | degrees | Yaw (heading) |
| Second | degrees | Pitch |
| Third | degrees | Roll |

**Rate:** 10 Hz (decimated from 50 Hz internal)

### Lift State

```
L:RAISED,1F
```

| Field | Description |
|-------|-------------|
| State | `IDLE`, `RAISING`, `RAISED`, `LOWERING`, `LOWERED`, `FAULT` |
| Magnet mask | Hex bitmask of energized magnets (bits 0-4) |

**Rate:** On state change

### Diagnostics

```
D:STATE=1,LOOPS=54321,CMDS=1234,TIMEOUTS=2,CYCLES=5432,LOOP_US=150/890,FLT=trlbv,SRAM=4200,UP=3600
```

| Field | Description |
|-------|-------------|
| `STATE` | System state enum value |
| `LOOPS` | Total loop() iterations |
| `CMDS` | Valid commands received |
| `TIMEOUTS` | Timeout events |
| `CYCLES` | PID computation cycles |
| `LOOP_US` | Average/max loop time in µs |
| `FLT` | Fault flags (uppercase = active) |
| `SRAM` | Free SRAM in bytes |
| `UP` | Uptime in seconds |

**Fault flag characters:**
- `T`/`t` — Command timeout
- `R`/`r` — Right encoder fault
- `L`/`l` — Left encoder fault
- `B`/`b` — Battery fault
- `V`/`v` — Velocity limit

**Rate:** Every 5 seconds

### Status Message

```
S:Minesweeper Motor Controller v2.0
```

Human-readable status text.  Sent on boot and on significant state changes.

### Error Message

```
E:3:IMU communication timeout
```

| Field | Description |
|-------|-------------|
| Code | Numeric error category |
| Message | Human-readable description |

**Error codes:**

| Code | Category |
|------|----------|
| 1 | Parser error |
| 2 | Safety system |
| 3 | IMU fault |
| 4 | Lift fault |
| 5 | Sensor fault |

---

## Parser State Machine

```
                    ┌────────────────────┐
                    │  WAITING_PREFIX    │◄───── Reset on error
                    └──┬───┬───┬────────┘
                       │   │   │
                  'r'/'l' 'C' other
                       │   │   │
                       │   │   └─ (ignore)
                       │   │
              ┌────────▼┐  ▼─────────────┐
              │READING  │  │  READING     │
              │DIRECTION│  │  COMMAND     │
              └──┬──────┘  └──┬───────────┘
            'p'/'n'          '\n'
              │               │
         ┌────▼──────┐   ┌───▼──────────┐
         │ READING   │   │ Dispatch     │
         │ VALUE     │   │ Extended Cmd │
         └──┬────────┘   └──────────────┘
         ','  or  '\n'
            │
    ┌───────▼────────┐
    │ Store value    │
    │ Check complete │
    └────────────────┘
```

---

## Error Handling

### Malformed Packets

- Any unexpected character resets the parser to `WAITING_PREFIX`.
- No error message is sent for malformed velocity commands (too frequent).
- Value buffer overflow (> 7 chars) resets the parser.

### Timeout

- If no valid command is received within `COMMAND_TIMEOUT_MS` (1500 ms):
  - All motors stop.
  - System transitions to `ESTOP_TIMEOUT`.
  - `SAFETY: Command timeout` message sent (if verbose).
- Recovery: The next valid velocity command clears the timeout.

### Buffer Overflow

- Receive buffer: 32 bytes (fixed, stack-allocated).
- Value buffer: 8 bytes (fixed, stack-allocated).
- If either would overflow, the parser resets.
- No dynamic memory is ever allocated.

---

## Timing

| Direction | Typical Latency | Max Latency |
|-----------|-----------------|-------------|
| Command RX → Motor output | 100 ms | 200 ms |
| Sensor read → Telemetry TX | 100 ms | 150 ms |
| Command → Acknowledgment | N/A | N/A (no ACK) |

---

## Future: Checksum

A CRC-8 checksum may be added in a future version:

```
rp02.50,ln01.30,*A3
```

Where `*A3` is a two-character hex CRC-8 of all preceding bytes.  The current protocol does not use checksums.  The parser will ignore `*XX` suffixes for forward compatibility.

---

## ROS 2 Integration Notes

The ROS 2 node on the Raspberry Pi should:

1. Open `/dev/ttyACM0` (or `/dev/ttyUSB0`) at 115200 baud.
2. Send velocity commands at 10 Hz minimum (to avoid timeout).
3. Parse telemetry lines by prefix character:
   - `r` → velocity telemetry
   - `O` → odometry
   - `M` → metal detector
   - `P` → proximity
   - `I` → IMU
   - `L` → lift state
   - `D` → diagnostics
   - `S` → status
   - `E` → error
4. Extended commands use the `C` prefix followed by newline-terminated strings.
5. The Arduino will print `MC:READY` on startup — wait for this before sending commands.
