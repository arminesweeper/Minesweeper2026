"""
Command Manager for Minesweeper Robot.
Arbitrates outgoing commands with strict priority, rate limiting, and watchdog streaming.
Priorities:
  1. Emergency Stop (highest)
  2. Neutral Stop
  3. Operator Movement Commands
  4. Extended Mechanism / Config Commands (lowest)
"""

import logging
import time
from typing import Callable, Optional
from .protocol.robot_protocol import RobotProtocol
from .safety_manager import SafetyManager

logger = logging.getLogger("ControlStation.CommandManager")


class CommandManager:
    """Arbitrates and rate-limits commands before passing to SerialWorker."""

    def __init__(
        self,
        protocol: RobotProtocol,
        safety: SafetyManager,
        send_fn: Callable[[bytes], None],
        rate_hz: float = 10.0,
    ):
        self.protocol = protocol
        self.safety = safety
        self.send_fn = send_fn
        self.min_period_s = 1.0 / max(1.0, min(50.0, rate_hz))

        self._active_target_r: float = 0.0
        self._active_target_l: float = 0.0
        self._gripper_cmd: Optional[float] = None
        self._servo_angle: Optional[float] = None

        self._last_send_time: float = 0.0
        self._is_moving: bool = False
        self._last_sent_r: float = 0.0
        self._last_sent_l: float = 0.0

    def trigger_estop(self) -> None:
        """
        Highest Priority: Send Emergency Stop immediately and stop movement.
        """
        self._active_target_r = 0.0
        self._active_target_l = 0.0
        self._is_moving = False
        self.safety.trigger_estop("Operator E-Stop command")

        # Send both ESTOP extended command and zero-velocity drive packet
        estop_bytes = self.protocol.encode_estop()
        stop_bytes = self.protocol.encode_stop()
        self.send_fn(estop_bytes)
        self.send_fn(stop_bytes)
        self._last_send_time = time.time()
        logger.warning("COMMAND: Dispatched ESTOP packets.")

    def request_stop(self) -> None:
        """
        Priority 2: Immediate neutral stop.
        """
        self._active_target_r = 0.0
        self._active_target_l = 0.0
        self._is_moving = False
        stop_bytes = self.protocol.encode_stop()
        self.send_fn(stop_bytes)
        self._last_sent_r = 0.0
        self._last_sent_l = 0.0
        self._last_send_time = time.time()
        logger.info("COMMAND: Dispatched STOP.")

    def set_drive_targets(
        self,
        right_vel: float,
        left_vel: float,
        gripper_vel: Optional[float] = None,
        servo_angle: Optional[float] = None,
        immediate: bool = False,
    ) -> bool:
        """
        Priority 3: Operator movement command.
        Checks safety authorization and rate limits.
        """
        if not self.safety.can_move():
            # If not authorized to move, force zero
            if self._is_moving:
                self.request_stop()
            return False

        self._active_target_r = right_vel
        self._active_target_l = left_vel
        self._gripper_cmd = gripper_vel
        self._servo_angle = servo_angle
        self._is_moving = (abs(right_vel) > 0.01 or abs(left_vel) > 0.01)

        now = time.time()
        # If moving and period elapsed, or immediate requested
        if immediate or (now - self._last_send_time >= self.min_period_s):
            self._dispatch_active_drive()
            return True
        return False

    def update_heartbeat(self) -> None:
        """
        Periodic tick called by timer (e.g. at 20 Hz).
        If robot is moving, transmits drive packet at ~10 Hz to satisfy the 1500 ms watchdog.
        """
        if not self.safety.can_move():
            if self._is_moving:
                self.request_stop()
            return

        if self._is_moving:
            now = time.time()
            if now - self._last_send_time >= self.min_period_s:
                self._dispatch_active_drive()

    def _dispatch_active_drive(self) -> None:
        """Encode and transmit active drive command."""
        pkt = self.protocol.encode_drive(
            self._active_target_r,
            self._active_target_l,
            self._gripper_cmd,
            self._servo_angle,
        )
        self.send_fn(pkt)
        self._last_sent_r = self._active_target_r
        self._last_sent_l = self._active_target_l
        self._last_send_time = time.time()

    def send_extended_command(self, cmd_bytes: bytes) -> bool:
        """
        Priority 4: Extended commands (lift, magnets, siren, PID, reset).
        Blocked during E-stop.
        """
        if self.safety.is_estop_latched:
            logger.warning("COMMAND: Rejected extended command because E-Stop is latched.")
            return False

        self.send_fn(cmd_bytes)
        return True
