"""
RobotProtocol: High-level protocol abstraction.
Coordinates command encoding, validation, and telemetry decoding.
"""

from typing import Any, List, Optional
from .command_encoder import CommandEncoder
from .telemetry_parser import TelemetryParser


class RobotProtocol:
    """Unified robot communication protocol handler."""

    BAUD_RATE = 115200
    WATCHDOG_TIMEOUT_MS = 1500  # Firmware auto-stops if no drive packet in 1500ms
    DEFAULT_MAX_VELOCITY = 10.0  # rad/s

    def __init__(self, max_velocity: float = DEFAULT_MAX_VELOCITY):
        self.encoder = CommandEncoder(max_velocity=max_velocity)
        self.parser = TelemetryParser()

    def encode_drive(
        self,
        right_vel: float,
        left_vel: float,
        gripper_vel: Optional[float] = None,
        servo_angle: Optional[float] = None,
    ) -> bytes:
        return self.encoder.encode_drive(right_vel, left_vel, gripper_vel, servo_angle)

    def encode_stop(self) -> bytes:
        return self.encoder.encode_stop()

    def encode_estop(self) -> bytes:
        return self.encoder.encode_estop()

    def encode_clear(self) -> bytes:
        return self.encoder.encode_clear_faults()

    def encode_reset(self) -> bytes:
        return self.encoder.encode_reset()

    def encode_pid(self, enable: bool) -> bytes:
        return self.encoder.encode_pid(enable)

    def encode_lift_cycle(self) -> bytes:
        return self.encoder.encode_lift_cycle()

    def encode_lift_lower(self) -> bytes:
        return self.encoder.encode_lift_lower()

    def encode_lift_stop(self) -> bytes:
        return self.encoder.encode_lift_stop()

    def encode_magnets(self, on: bool) -> bytes:
        return self.encoder.encode_magnets(on)

    def encode_siren(self, on: bool) -> bytes:
        return self.encoder.encode_siren(on)

    def encode_servo(self, angle: int) -> bytes:
        return self.encoder.encode_servo(angle)

    def decode_chunk(self, chunk: str) -> List[Any]:
        """Feed incoming serial chunk and return list of parsed packets."""
        return self.parser.feed(chunk)

    def reset_parser(self) -> None:
        self.parser.reset()
