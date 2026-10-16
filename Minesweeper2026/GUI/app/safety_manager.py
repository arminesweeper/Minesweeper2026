"""
Safety Manager for Minesweeper Robot.
Enforces arming interlocks, software emergency stop latches,
heartbeat staleness detection, and safe transition policies.
"""

import logging
import time
from typing import Callable, Optional
from .robot_state import RobotState, SafetyState, ConnectionState

logger = logging.getLogger("ControlStation.SafetyManager")


class SafetyManager:
    """Oversees system safety state transitions and movement authorization."""

    def __init__(
        self,
        robot_state: RobotState,
        stale_timeout_s: float = 1.5,
        on_safety_changed: Optional[Callable[[SafetyState], None]] = None,
    ):
        self.state = robot_state
        self.stale_timeout_s = stale_timeout_s
        self.on_safety_changed = on_safety_changed
        self._armed = False
        self._estop_latched = False

    @property
    def is_armed(self) -> bool:
        return self._armed and not self._estop_latched

    @property
    def is_estop_latched(self) -> bool:
        return self._estop_latched

    def trigger_estop(self, reason: str = "Operator E-Stop") -> None:
        """
        Engage Emergency Stop latch.
        Disarms the robot and prevents all movement commands.
        """
        self._estop_latched = True
        self._armed = False
        self.state.estop_latched = True
        self.state.safety_state = SafetyState.ESTOP
        logger.warning("SAFETY: Emergency Stop triggered: %s", reason)
        if self.on_safety_changed:
            self.on_safety_changed(SafetyState.ESTOP)

    def reset_estop_latch(self) -> bool:
        """
        Operator deliberately resets the E-stop latch.
        Robot remains DISARMED until explicit arming.
        """
        if not self._estop_latched:
            return True

        self._estop_latched = False
        self.state.estop_latched = False
        self.state.safety_state = SafetyState.DISARMED
        logger.info("SAFETY: Emergency stop latch reset. Robot is DISARMED.")
        if self.on_safety_changed:
            self.on_safety_changed(SafetyState.DISARMED)
        return True

    def arm(self) -> bool:
        """
        Arm the robot for movement.
        Requires connection, unlatched e-stop, and fresh communication.
        """
        if self._estop_latched:
            logger.warning("SAFETY: Cannot arm while E-Stop is latched.")
            return False

        if self.state.connection_state != ConnectionState.CONNECTED:
            logger.warning("SAFETY: Cannot arm while disconnected.")
            return False

        self._armed = True
        self.state.safety_state = SafetyState.ARMED
        logger.info("SAFETY: Robot ARMED.")
        if self.on_safety_changed:
            self.on_safety_changed(SafetyState.ARMED)
        return True

    def disarm(self) -> None:
        """Disarm the robot. Movement requests will be rejected."""
        self._armed = False
        if not self._estop_latched:
            self.state.safety_state = SafetyState.DISARMED
            if self.on_safety_changed:
                self.on_safety_changed(SafetyState.DISARMED)
        logger.info("SAFETY: Robot DISARMED.")

    def check_staleness(self) -> bool:
        """
        Verify that telemetry is actively arriving.
        If communication is stale (> stale_timeout_s), disarms robot.
        """
        if self.state.is_communication_stale(self.stale_timeout_s):
            if self._armed:
                logger.warning("SAFETY: Telemetry stale (>%.1fs). Disarming robot.", self.stale_timeout_s)
                self.disarm()
            return True
        return False

    def can_move(self) -> bool:
        """Check if movement commands are currently authorized."""
        if self._estop_latched:
            return False
        if not self._armed:
            return False
        if self.state.connection_state != ConnectionState.CONNECTED:
            return False
        return True
