"""
Robot State definitions and thread-safe telemetry container.
Maintains the single source of truth for the control station.
"""

from dataclasses import dataclass, field
from enum import Enum
import threading
import time
from typing import List, Optional


class ConnectionState(Enum):
    DISCONNECTED = "Disconnected"
    CONNECTING = "Connecting"
    CONNECTED = "Connected"
    RECONNECTING = "Reconnecting"
    ERROR = "Connection Error"


class SafetyState(Enum):
    DISARMED = "DISARMED"
    ARMED = "ARMED"
    ESTOP = "EMERGENCY STOP"
    FAULT = "FAULT"


@dataclass
class DriveTelemetry:
    measured_right: float = 0.0
    measured_left: float = 0.0
    target_right: float = 0.0
    target_left: float = 0.0
    timestamp: float = 0.0


@dataclass
class OdometryTelemetry:
    x: float = 0.0
    y: float = 0.0
    theta: float = 0.0
    timestamp: float = 0.0


@dataclass
class BatteryTelemetry:
    voltage: float = 0.0
    percentage: float = 0.0
    is_low: bool = False
    is_critical: bool = False
    timestamp: float = 0.0

    @classmethod
    def from_voltage(cls, v: float, now: float) -> "BatteryTelemetry":
        # 3S LiPo: Nominal 11.1V, Full 12.6V, Low 10.8V, Critical 9.6V
        if v <= 5.0:
            pct = 0.0  # Disconnected or unpowered
        else:
            pct = max(0.0, min(100.0, (v - 9.6) / (12.6 - 9.6) * 100.0))
        return cls(
            voltage=v,
            percentage=pct,
            is_low=(v < 10.8 and v > 5.0),
            is_critical=(v <= 9.6 and v > 5.0),
            timestamp=now,
        )


@dataclass
class IMUTelemetry:
    yaw: float = 0.0
    pitch: float = 0.0
    roll: float = 0.0
    timestamp: float = 0.0


@dataclass
class SensorsTelemetry:
    proximity: List[int] = field(default_factory=lambda: [0, 0, 0, 0, 0])
    metal_detected: bool = False
    proximity_triggered: bool = False
    timestamp: float = 0.0


@dataclass
class LiftTelemetry:
    state_str: str = "IDLE"
    magnet_mask: int = 0
    gripper_state: str = "c"  # 'o'=open/top, 'c'=closed/bottom, 'm'=moving
    timestamp: float = 0.0


@dataclass
class DiagnosticsTelemetry:
    loop_hz: int = 0
    cpu_percent: int = 0
    fault_code: int = 0
    timestamp: float = 0.0


class RobotState:
    """Thread-safe snapshot and live telemetry coordinator."""

    def __init__(self):
        self._lock = threading.RLock()

        self.connection_state: ConnectionState = ConnectionState.DISCONNECTED
        self.safety_state: SafetyState = SafetyState.DISARMED
        self.selected_port: str = ""
        self.active_baud: int = 115200

        self.drive: DriveTelemetry = DriveTelemetry()
        self.odometry: OdometryTelemetry = OdometryTelemetry()
        self.battery: BatteryTelemetry = BatteryTelemetry()
        self.imu: IMUTelemetry = IMUTelemetry()
        self.sensors: SensorsTelemetry = SensorsTelemetry()
        self.lift: LiftTelemetry = LiftTelemetry()
        self.diagnostics: DiagnosticsTelemetry = DiagnosticsTelemetry()

        self.last_tx_time: float = 0.0
        self.last_rx_time: float = 0.0
        self.last_command_sent: str = ""
        self.last_status_message: str = ""
        self.last_error_message: str = ""

        # Flags for latching GUI stop state
        self.estop_latched: bool = False
        self.unconfirmed_stop: bool = False

    def update_drive_telemetry(self, measured_r: float, measured_l: float, gripper: str) -> None:
        with self._lock:
            now = time.time()
            self.drive.measured_right = measured_r
            self.drive.measured_left = measured_l
            self.drive.timestamp = now
            self.lift.gripper_state = gripper
            self.lift.timestamp = now
            self.last_rx_time = now

    def update_odometry(self, x: float, y: float, theta: float) -> None:
        with self._lock:
            now = time.time()
            self.odometry = OdometryTelemetry(x=x, y=y, theta=theta, timestamp=now)
            self.last_rx_time = now

    def update_battery(self, voltage: float) -> None:
        with self._lock:
            now = time.time()
            self.battery = BatteryTelemetry.from_voltage(voltage, now)
            self.last_rx_time = now

    def update_imu(self, yaw: float, pitch: float, roll: float) -> None:
        with self._lock:
            now = time.time()
            self.imu = IMUTelemetry(yaw=yaw, pitch=pitch, roll=roll, timestamp=now)
            self.last_rx_time = now

    def update_proximity(self, values: List[int]) -> None:
        with self._lock:
            now = time.time()
            self.sensors.proximity = list(values)
            self.sensors.proximity_triggered = any(v > 500 for v in values)
            self.sensors.timestamp = now
            self.last_rx_time = now

    def update_metal(self, detected: bool) -> None:
        with self._lock:
            now = time.time()
            self.sensors.metal_detected = detected
            self.sensors.timestamp = now
            self.last_rx_time = now

    def update_lift_state(self, state_str: str, magnet_mask: int) -> None:
        with self._lock:
            now = time.time()
            self.lift.state_str = state_str
            self.lift.magnet_mask = magnet_mask
            self.lift.timestamp = now
            self.last_rx_time = now

    def update_diagnostics(self, loop_hz: int, cpu: int, fault_code: int) -> None:
        with self._lock:
            now = time.time()
            self.diagnostics = DiagnosticsTelemetry(
                loop_hz=loop_hz, cpu_percent=cpu, fault_code=fault_code, timestamp=now
            )
            self.last_rx_time = now

    def set_status_message(self, msg: str) -> None:
        with self._lock:
            self.last_status_message = msg
            self.last_rx_time = time.time()

    def set_error_message(self, err: str) -> None:
        with self._lock:
            self.last_error_message = err
            self.last_rx_time = time.time()

    def record_command_sent(self, cmd_str: str, target_r: float = 0.0, target_l: float = 0.0) -> None:
        with self._lock:
            self.last_tx_time = time.time()
            self.last_command_sent = cmd_str
            self.drive.target_right = target_r
            self.drive.target_left = target_l

    def is_communication_stale(self, timeout_s: float = 1.5) -> bool:
        with self._lock:
            if self.connection_state != ConnectionState.CONNECTED:
                return True
            if self.last_rx_time == 0.0:
                return False
            return (time.time() - self.last_rx_time) > timeout_s
