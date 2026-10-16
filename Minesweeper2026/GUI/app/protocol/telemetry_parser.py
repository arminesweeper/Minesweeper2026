"""
Telemetry Parser for Minesweeper Robot.
Parses streamed serial data from Arduino Mega into structured telemetry objects.
Handles message fragmentation, multiple packets per read, and malformed lines safely.
"""

from dataclasses import dataclass
import logging
import re
from typing import Any, Callable, Dict, List, Optional, Tuple

logger = logging.getLogger("ControlStation.TelemetryParser")


@dataclass
class HighRateTelemetryPacket:
    measured_right: float
    measured_left: float
    gripper_state: str  # 'o', 'c', 'm'


@dataclass
class OdometryPacket:
    x: float
    y: float
    theta: float


@dataclass
class BatteryPacket:
    voltage: float


@dataclass
class IMUPacket:
    yaw: float
    pitch: float
    roll: float


@dataclass
class ProximityPacket:
    values: List[int]


@dataclass
class MetalDetectPacket:
    detected: bool


@dataclass
class LiftStatePacket:
    state_str: str
    magnet_mask: int


@dataclass
class UltrasonicPacket:
    distance_cm: int


@dataclass
class DiagnosticsPacket:
    loop_hz: int
    cpu_percent: int
    fault_code: int


@dataclass
class StatusPacket:
    message: str


@dataclass
class ErrorPacket:
    code: int
    message: str


class TelemetryParser:
    """Stream parser for Arduino Mega serial output."""

    MAX_BUFFER_SIZE = 8192

    # Regex for High-Rate telemetry: r[pn]\d+(\.\d+)?,l[pn]\d+(\.\d+)?,g[ocm],
    _RE_HIGH_RATE = re.compile(
        r"r([pn])([\d\.]+),l([pn])([\d\.]+),g([ocm]),?"
    )

    def __init__(self):
        self._buffer: str = ""

    def reset(self) -> None:
        """Clear parser buffer."""
        self._buffer = ""

    def feed(self, data: str) -> List[Any]:
        """
        Feed incoming raw string chunk and return list of parsed packets.
        Handles fragments, complete lines, and trailing characters.
        """
        if not data:
            return []

        self._buffer += data
        if len(self._buffer) > self.MAX_BUFFER_SIZE:
            # Prevent memory overflow on garbage data by keeping only the tail
            logger.warning("Parser buffer overflow; trimming oldest bytes.")
            self._buffer = self._buffer[-2048:]

        packets: List[Any] = []

        # Split on newline delimiters
        lines = self._buffer.split("\n")
        # The last element is the remaining incomplete line (if any)
        self._buffer = lines.pop()

        for line in lines:
            line = line.strip("\r\n\0 ")
            if not line:
                continue

            parsed = self.parse_line(line)
            if parsed is not None:
                packets.append(parsed)

        return packets

    def parse_line(self, line: str) -> Optional[Any]:
        """Parse a single complete line from the serial stream."""
        if not line:
            return None

        try:
            # 1. High-rate Drive Telemetry: rp0.00,lp0.00,go,
            hr_match = self._RE_HIGH_RATE.search(line)
            if hr_match:
                r_sign, r_val_str, l_sign, l_val_str, gripper = hr_match.groups()
                r_val = float(r_val_str)
                if r_sign == "n":
                    r_val = -r_val
                l_val = float(l_val_str)
                if l_sign == "n":
                    l_val = -l_val
                return HighRateTelemetryPacket(
                    measured_right=r_val,
                    measured_left=l_val,
                    gripper_state=gripper,
                )

            # 2. Odometry: O:x,y,theta
            if line.startswith("O:"):
                parts = line[2:].split(",")
                if len(parts) == 3:
                    return OdometryPacket(
                        x=float(parts[0]),
                        y=float(parts[1]),
                        theta=float(parts[2]),
                    )

            # 3. Battery: B:volts
            if line.startswith("B:"):
                val_str = line[2:].strip()
                return BatteryPacket(voltage=float(val_str))

            # 4. IMU: I:yaw,pitch,roll
            if line.startswith("I:"):
                parts = line[2:].split(",")
                if len(parts) == 3:
                    return IMUPacket(
                        yaw=float(parts[0]),
                        pitch=float(parts[1]),
                        roll=float(parts[2]),
                    )

            # 5. Proximity: P:s0,s1,s2,s3,s4
            if line.startswith("P:"):
                parts = line[2:].split(",")
                if len(parts) == 5:
                    return ProximityPacket(values=[int(p) for p in parts])

            # 6. Metal Detector: M:1 or M:0
            if line.startswith("M:"):
                flag_char = line[2:].strip()
                return MetalDetectPacket(detected=(flag_char == "1"))

            # 7. Lift State: L:state_str,magnet_mask_hex
            if line.startswith("L:"):
                parts = line[2:].split(",")
                if len(parts) == 2:
                    mask_val = int(parts[1].strip(), 16) if parts[1].strip() else 0
                    return LiftStatePacket(state_str=parts[0].strip(), magnet_mask=mask_val)

            # 8. Ultrasonic Distance: U:dist_cm
            if line.startswith("U:"):
                dist_str = line[2:].strip()
                if dist_str.isdigit():
                    return UltrasonicPacket(distance_cm=int(dist_str))

            # 9. Diagnostics: D:hz,cpu,fault
            if line.startswith("D:"):
                parts = line[2:].split(",")
                if len(parts) == 3:
                    return DiagnosticsPacket(
                        loop_hz=int(parts[0]),
                        cpu_percent=int(parts[1]),
                        fault_code=int(parts[2]),
                    )

            # 9. Status message: S:msg
            if line.startswith("S:"):
                return StatusPacket(message=line[2:].strip())

            # 10. Error message: E:code:msg
            if line.startswith("E:"):
                parts = line[2:].split(":", 1)
                if len(parts) == 2:
                    return ErrorPacket(code=int(parts[0]), message=parts[1].strip())
                elif len(parts) == 1:
                    return ErrorPacket(code=-1, message=parts[0].strip())

            # 11. Boot / Banner string
            if "READY" in line or "Motor Controller" in line:
                return StatusPacket(message=f"Firmware Boot: {line}")

        except (ValueError, IndexError) as ex:
            logger.debug("Failed parsing telemetry line %r: %s", line, ex)

        return None
