"""UI panels package for Minesweeper Control Station."""

from .connection_panel import ConnectionPanel
from .safety_panel import SafetyPanel
from .movement_panel import MovementPanel
from .speed_panel import SpeedPanel
from .motor_test_panel import MotorTestPanel
from .mechanism_panel import MechanismPanel
from .telemetry_panel import TelemetryPanel
from .serial_console import SerialConsole

__all__ = [
    "ConnectionPanel",
    "SafetyPanel",
    "MovementPanel",
    "SpeedPanel",
    "MotorTestPanel",
    "MechanismPanel",
    "TelemetryPanel",
    "SerialConsole",
]
