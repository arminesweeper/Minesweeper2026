# Minesweeper Robot — Python Serial Control Station

Professional desktop GUI to control the Minesweeper competition robot through a direct USB-to-serial connection to its **Arduino Mega 2560** controller.

Built with **Python 3**, **PySide6**, and **PySerial**. Fully grounded in the actual robot firmware (`SerialProtocol.cpp`, `SystemManager.cpp`, and `Config.h`).

---

## 1. System Architecture

```text
┌────────────────────────────────────────────────────────┐
│             Python Desktop GUI (PySide6)               │
│                                                        │
│  ┌──────────────────┐  ┌────────────────────────────┐  │
│  │ Connection Panel │  │  Safety & E-Stop Interlock │  │
│  └────────┬─────────┘  └─────────────┬──────────────┘  │
│           │                          │                 │
│  ┌────────▼──────────────────────────▼──────────────┐  │
│  │ ApplicationController / CommandManager (10 Hz)   │  │
│  └────────────────────────┬─────────────────────────┘  │
│                           │ Thread-Safe Queues         │
│  ┌────────────────────────▼─────────────────────────┐  │
│  │ SerialWorker (Dedicated QThread / PySerial)      │  │
│  └────────────────────────┬─────────────────────────┘  │
└───────────────────────────┼────────────────────────────┘
                            │ USB Serial (115200 Baud, 8N1)
                            ▼
┌────────────────────────────────────────────────────────┐
│               Arduino Mega 2560 (MCU)                  │
│                                                        │
│  ┌──────────────────────────────────────────────────┐  │
│  │ SerialProtocol Parser (Non-blocking)             │  │
│  │ Watchdog Monitor (Auto-stops if no packet in 1.5s)│  │
│  └────────────────────────┬─────────────────────────┘  │
│  ┌────────────────────────▼─────────────────────────┐  │
│  │ Motion Controller / Dual Wheel PID Controllers   │  │
│  └────────────────────────┬─────────────────────────┘  │
│  ┌────────────────────────▼─────────────────────────┐  │
│  │ Cytron MDD10A Motor Driver & Drive Motors        │  │
│  └──────────────────────────────────────────────────┘  │
└────────────────────────────────────────────────────────┘
```

---

## 2. Verified Communication Protocol Specification

Derived directly from `SerialProtocol.cpp` and `SystemManager.cpp`.

### Serial Parameters
- **Baud Rate:** `115200` bps
- **Data Bits:** 8, **Parity:** None, **Stop Bits:** 1 (8N1)
- **Flow Control:** None
- **Command Watchdog:** `1500 ms` (`COMMAND_TIMEOUT_MS`). The MCU halts all drive motors and engages internal `ESTOP` if no valid velocity packet arrives within 1.5 seconds.
- **DTR Behavior:** Opening the serial port asserts DTR, resetting the ATmega2560. The GUI waits for initialization before enabling commands.

### A. Inbound Movement & Velocity Packets (GUI → Mega)
- **Syntax:** `r<sign><XX.XX>,l<sign><XX.XX>,[g<sign><XX.XX>,][s<sign><XX.XX>,]\n`
  - `sign`: `'p'` for positive (forward/open), `'n'` for negative (reverse/close).
  - `XX.XX`: Float value with 2 decimal places (clamped to max `10.00` rad/s).
  - Delimiters: Comma-separated with trailing comma and newline `\n`.
  - `r`: Right wheel velocity in rad/s (range `-10.00` to `+10.00`).
  - `l`: Left wheel velocity in rad/s (range `-10.00` to `+10.00`).
  - `g`: Gripper velocity command (`gp01.00` = trigger pickup cycle, `gn01.00` = lower, `gp00.00` = stop).
  - `s`: Camera servo pan angle (`sp90.00` = center, range `sp00.00` to `sp180.00`).
- **Examples:**
  - Neutral Stop: `rp00.00,lp00.00,\n`
  - Forward (3.0 rad/s): `rp03.00,lp03.00,\n`
  - Reverse (3.0 rad/s): `rn03.00,ln03.00,\n`
  - Turn Left (In-place): `rp02.50,ln02.50,\n`
  - Turn Right (In-place): `rn02.50,lp02.50,\n`

### B. Inbound Extended System Commands (GUI → Mega)
Prefix `C` followed by command name and newline `\n`:
- `CESTOP\n` — Immediate software Emergency Stop (halts motors, sets firmware state to `ESTOP`).
- `CCLEAR\n` — Clears firmware emergency stop latch and resets system faults.
- `CRESET\n` — Resets wheel odometry (X, Y, θ), zeros IMU yaw, and clears diagnostic counters.
- `CPID:ON\n` — Enables closed-loop PID velocity control with encoder feedback.
- `CPID:OFF\n` — Disables PID, switching to open-loop direct PWM control.
- `CLIFT:CYCLE\n` / `CLIFT:UP\n` — Triggers automated mine pickup and drop cycle.
- `CLIFT:DN\n` — Lowers lift actuator until bottom limit switch triggers.
- `CLIFT:STOP\n` — Halts lift motor immediately.
- `CMAGNET:ON\n` — Energizes all electromagnets via PCB relay (Pin 38).
- `CMAGNET:OFF\n` — De-energizes all electromagnets.
- `CBUZZ:ALERT\n` — Turns on 12V high-output alert siren relay (Pin 47).
- `CBUZZ:SILENT\n` — Turns off siren.
- `CSERVO:<angle>\n` — Sets camera pan servo angle (0 to 180 degrees, Pin 4).

### C. Outbound Telemetry (Mega → GUI)
- **High-Rate Telemetry (10 Hz):**
  - Format: `r<sign><val>,l<sign><val>,g<state>,`
  - `r` / `l`: Measured wheel angular velocity in rad/s.
  - `g`: Discrete gripper state: `'o'` (at top/open), `'c'` (at bottom/closed), `'m'` (moving/cycling).
- **Low-Rate Telemetry Phase 0 (2 Hz):**
  - Odometry: `O:<x>,<y>,<theta>` (meters, meters, radians).
  - Battery: `B:<volts>` (e.g. `B:11.85`).
- **Low-Rate Telemetry Phase 1 (2 Hz):**
  - IMU: `I:<yaw>,<pitch>,<roll>` (degrees).
  - Proximity Array: `P:<s0>,<s1>,<s2>,<s3>,<s4>` (ADC raw 0–1023; threshold is 500).
- **Asynchronous Events:**
  - Metal Detection: `M:1` (metal detected) / `M:0` (cleared).
  - Lift State: `L:<state_str>,<magnet_mask_hex>` (e.g. `L:RAISING,1F`).
  - Diagnostics: `D:<hz>,<cpu>,<fault_code>` (e.g. `D:50,0,0`).
  - Status Messages: `S:<text>` (e.g. `S:PID Enabled`).
  - Error Messages: `E:<code>:<description>`.

---

## 3. Installation & Setup (Windows)

### Prerequisites
- Python 3.10, 3.11, 3.12, 3.13, or 3.14 (64-bit).
- Arduino Mega 2560 connected via USB cable (or run in `--sim` mode).

### Step 1: Install Dependencies
Open PowerShell or Command Prompt in `Minesweeper2026/GUI`:
```powershell
pip install -r requirements.txt
```

### Step 2: Run the Control Station
```powershell
python main.py
```

### Simulation Mode (No Hardware Required)
Test all GUI controls, safety latches, and simulated telemetry without physical hardware:
```powershell
python main.py --sim
```
Or check the **Simulation Mode** checkbox directly in the GUI.

---

## 4. Identifying the Correct COM Port

1. Connect the Arduino Mega 2560 to your PC via USB.
2. In Windows, open **Device Manager** (`devmgmt.msc`).
3. Expand **Ports (COM & LPT)**.
4. Look for:
   - `Arduino Mega 2560 (COMx)` (for official/16U2 boards).
   - `USB-SERIAL CH340 (COMx)` (for compatible boards).
5. In the GUI, click **Refresh** and select the identified COM port.

---

## 5. Keyboard Teleoperation Reference

| Key | Action | Behavior |
| :--- | :--- | :--- |
| **W** | Move Forward | Drives both wheels forward at selected speed |
| **S** | Move Backward | Drives both wheels in reverse at selected speed |
| **A** | Turn Left | Spins robot left (Right wheel forward, Left wheel reverse) |
| **D** | Turn Right | Spins robot right (Right wheel reverse, Left wheel forward) |
| **Space** | Stop / Neutral | Immediately halts drive motors (`rp00.00,lp00.00,\n`) |
| **Esc** | Emergency Stop | Engages software E-Stop latch and cuts all motors |

*Safety Features:*
- Releasing a key immediately transmits a neutral stop packet.
- Losing window focus (alt-tab or clicking another app) clears held keys and stops the robot.
- Keystrokes are ignored when editing text fields or console filters.

---

## 6. Safety Interlocks & Emergency Stop

1. **Disarmed at Startup:** The robot starts in a `DISARMED` state. Movement commands cannot be transmitted until the operator clicks **ARM ROBOT**.
2. **Emergency Stop (E-Stop):**
   - Clicking the prominent **EMERGENCY STOP** button (or pressing `Space`/`Esc`) immediately transmits `CESTOP\n` and `rp00.00,lp00.00,\n`.
   - The GUI latches into `ESTOP` state. All movement requests are blocked.
   - To resume, the operator must deliberately click **Reset E-Stop**, which sends `CCLEAR\n` and returns to `DISARMED`, requiring a fresh **ARM ROBOT** confirmation.
3. **Communication Staleness Watchdog:**
   - If incoming serial telemetry stops for longer than 1.5 seconds, the GUI automatically marks the link `STALE` and disarms the robot.
4. **Safety Disclaimer:**
   > **Important:** Software E-Stop stops motors via serial communication. A true safety-rated emergency stop requires a physical kill switch that physically disconnects power from the Cytron MDD10A motor drivers.

---

## 7. Running the Automated Test Suite

Run the full automated test suite (34 unit and GUI tests):
```powershell
python -m pytest tests -v
```

Tests cover:
- Protocol encoding and command syntax.
- Value boundary clamping and sign formatting.
- Streaming telemetry parsing and fragmented read reassembly.
- Command arbitration, prioritization, and rate limiting.
- Safety state transitions, arming interlocks, and staleness detection.
- PySide6 GUI integration and button signals.

---

## 8. Standalone Executable Packaging (PyInstaller)

To bundle the application into a standalone Windows `.exe`:
```powershell
pip install pyinstaller
pyinstaller --name "MinesweeperControlStation" --windowed --noconfirm main.py
```
The output executable will be created in `dist/MinesweeperControlStation/`.
