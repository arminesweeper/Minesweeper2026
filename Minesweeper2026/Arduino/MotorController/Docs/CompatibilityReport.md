# Firmware / PCB Compatibility Report

**Project:** Minesweeper 2026  
**PCB:** `Minesweeper2026/Schematic/Robotics_Mine` (+ `mine_sch.pdf`) — **frozen, no copper edits**  
**Firmware:** `Minesweeper2026/Arduino/MotorController`  
**Report date:** 2026-08-05  
**Config revision:** EEPROM param VERSION **3**

---

## Executive Verdict

| Item | Status |
|------|--------|
| As-received firmware vs as-fabricated PCB | **Incompatible** |
| After VERSION 3 sync (firmware + harness only) | **Bring-up ready** |
| PCB copper changes | **Not required / not allowed** |
| Future clean layout (optional) | PCB Rev B when schedule allows |

---

## Engineering Decision (No PCB Edit)

**Constraint:** PCB cannot be cut, reworked, or re-spun.

**Strategy:** Match firmware to as-built copper. Resolve the ENC1↔I2C conflict in **firmware + flying leads**, not in copper.

| Area | Choice | Why |
|------|--------|-----|
| Drive PWM/DIR | D44/D42, D46/D40 | PCB copper |
| Encoders | D21/D20 (left), D19/D18 (right) | PCB copper as fabricated |
| IMU | Soft I2C on **D30/D31** | Avoids hardware Wire on D20/D21 |
| MPU connector J29/J38 | **Unused for data** | Those pads share ENC1 nets via Mega |
| Lift / limits / prox / metal / siren | Jumper harness | Headers exist; not on Mega pads |
| Magnets | Shared relay D38 | Matches K1 |
| Siren | Digital D37 | Matches K2 |
| Limits | Active HIGH | Matches 12 V divider |
| Battery / analog | Add-on to A0–A5 | Shield has no analog sockets |

---

## Critical Issues (Must Fix — Without PCB Edits)

| ID | Issue | Resolution |
|----|-------|------------|
| C1 | ENC1 on D20/D21 vs hardware I2C | Soft I2C IMU on D30/D31; leave J29 data unused |
| C2 | Drive pin mismatch | `Config.h` → D44/D42/D46/D40 |
| C3 | Lift/limits not on Mega pads | Jumpers D10/D8/D28/D29 |
| C4 | 5× magnet GPIO vs 1× relay | `MAGNET_SHARED_RELAY` |
| C5 | Limit polarity | `LIMIT_SWITCH_ACTIVE_HIGH` |
| C6 | Build breakers in SystemManager/Encoder | Fixed |

---

## Recommended Improvements (Harness / Firmware Only)

| ID | Recommendation |
|----|----------------|
| R1 | Strain-relieve D30/D31 IMU flying leads |
| R2 | Add 4.7 kΩ pull-ups on soft I2C if module lacks them |
| R3 | Battery divider harness to A0 |
| R4 | Label jumper harness ends with Mega pin numbers |
| R5 | Confirm whether K1 gates `12V_MG` during bring-up |

---

## Optional (Future PCB Rev B — When Allowed)

- Route ENC1 off D20/D21; restore hardware Wire
- Break out A0–A5 on shield
- Route lift/limits/siren/metal to Mega pads
- Per-channel magnet drivers

---

## Pin Conflict Matrix (VERSION 3)

| Resource | Status |
|----------|--------|
| Timer 0 | Clear |
| Timer 2 (D10 lift) | Clear |
| Timer 5 (D44/D46 drive) | Clear |
| INT2 (D19, D21) encoders | Clear |
| Hardware Wire D20/D21 | **Not used** for IMU |
| Soft I2C D30/D31 | Clear |
| UART0 USB | Clear |

---

## Files in This Sync

| File | Role |
|------|------|
| `Config.h` | VERSION 3 pins + `IMU_USE_SOFT_I2C` |
| `IMU.cpp` | Bit-bang I2C path |
| `Docs/Pinout.md` | As-fabricated map |
| `Docs/WiringManual.md` | Harness-only assembly |
| `Docs/CommissioningChecklist.md` | No-cut continuity checks |
| `Docs/CompatibilityReport.md` | This report |

---

## Bottom Line

Assemble and test with **firmware VERSION 3**, **encoders on shield copper as built**, **MPU on flying leads to D30/D31**, and the **jumper harness** for unrouted functions. Do not cut the PCB.
