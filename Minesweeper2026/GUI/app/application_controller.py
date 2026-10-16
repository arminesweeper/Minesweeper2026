"""
Application Controller: Central coordinator for the Minesweeper Control Station.
Binds background serial threads, safety policies, command rate limiting, and UI events.
"""

import logging
from typing import Optional
from PySide6.QtCore import QObject, QThread, QTimer, Signal, Slot

from .command_manager import CommandManager
from .logging_config import setup_logging
from .protocol import (
    BatteryPacket,
    DiagnosticsPacket,
    ErrorPacket,
    HighRateTelemetryPacket,
    IMUPacket,
    LiftStatePacket,
    MetalDetectPacket,
    OdometryPacket,
    ProximityPacket,
    RobotProtocol,
    StatusPacket,
    UltrasonicPacket,
)
from .robot_state import ConnectionState, RobotState, SafetyState
from .safety_manager import SafetyManager
from .serial_worker import SerialWorker
from .settings_manager import SettingsManager

logger = logging.getLogger("ControlStation.AppController")


class ApplicationController(QObject):
    """Central facade linking UI panels to hardware/safety infrastructure."""

    # UI notification signals
    connection_status_changed = Signal(str, str)
    safety_status_changed = Signal(str)
    telemetry_updated = Signal()
    console_message_received = Signal(str, str)  # direction ("TX"/"RX"), text
    error_message_raised = Signal(str)

    def __init__(self, settings: SettingsManager, simulation: bool = False):
        super().__init__()
        self.settings = settings
        self.simulation_mode = simulation

        # Core components
        self.robot_state = RobotState()
        self.protocol = RobotProtocol(
            max_velocity=self.settings.get("limits", "max_velocity_rad_s", 10.0)
        )
        self.safety = SafetyManager(
            robot_state=self.robot_state,
            stale_timeout_s=self.settings.get("limits", "stale_timeout_s", 1.5),
            on_safety_changed=self._on_safety_state_changed,
        )

        # Worker & Thread
        self._thread: Optional[QThread] = None
        self._worker: Optional[SerialWorker] = None

        # Setup Worker
        self._setup_worker()

        # Command Manager
        rate_hz = self.settings.get("limits", "command_rate_hz", 10.0)
        self.command_manager = CommandManager(
            protocol=self.protocol,
            safety=self.safety,
            send_fn=self._worker.send_command,
            rate_hz=rate_hz,
        )

        # Periodic coordination timer (20 Hz = 50ms)
        self._tick_timer = QTimer(self)
        self._tick_timer.timeout.connect(self._on_periodic_tick)
        self._tick_timer.start(50)

    def _setup_worker(self) -> None:
        """Create and start the background serial worker thread."""
        self._thread = QThread()
        self._worker = SerialWorker(simulation_mode=self.simulation_mode)
        self._worker.moveToThread(self._thread)

        # Connect thread lifecycle
        self._thread.started.connect(self._worker.run_loop)

        # Connect worker signals
        self._worker.connection_changed.connect(self._on_worker_connection_changed)
        self._worker.data_received.connect(self._on_raw_rx)
        self._worker.data_sent.connect(self._on_raw_tx)
        self._worker.telemetry_packet.connect(self._on_telemetry_packet)
        self._worker.error_occurred.connect(self._on_worker_error)

        self._thread.start()
        logger.info("Serial background thread started.")

    @Slot(str, str)
    def _on_worker_connection_changed(self, status: str, details: str) -> None:
        """Update robot state connection mode and notify UI."""
        if status == "Connected":
            self.robot_state.connection_state = ConnectionState.CONNECTED
        elif status == "Connecting":
            self.robot_state.connection_state = ConnectionState.CONNECTING
        elif status == "Reconnecting":
            self.robot_state.connection_state = ConnectionState.RECONNECTING
        elif status == "Connection Error":
            self.robot_state.connection_state = ConnectionState.ERROR
            self.safety.disarm()
            self.command_manager.request_stop()
        else:
            self.robot_state.connection_state = ConnectionState.DISCONNECTED
            self.safety.disarm()
            self.command_manager.request_stop()

        self.connection_status_changed.emit(status, details)
        self.telemetry_updated.emit()

    @Slot(str)
    def _on_raw_rx(self, text: str) -> None:
        self.console_message_received.emit("RX", text)

    @Slot(str)
    def _on_raw_tx(self, text: str) -> None:
        self.console_message_received.emit("TX", text)

    @Slot(str)
    def _on_worker_error(self, err: str) -> None:
        self.robot_state.set_error_message(err)
        self.error_message_raised.emit(err)

    @Slot(object)
    def _on_telemetry_packet(self, pkt: object) -> None:
        """Dispatch parsed telemetry to RobotState and trigger UI refresh."""
        if isinstance(pkt, HighRateTelemetryPacket):
            self.robot_state.update_drive_telemetry(
                pkt.measured_right, pkt.measured_left, pkt.gripper_state
            )
        elif isinstance(pkt, OdometryPacket):
            self.robot_state.update_odometry(pkt.x, pkt.y, pkt.theta)
        elif isinstance(pkt, BatteryPacket):
            self.robot_state.update_battery(pkt.voltage)
        elif isinstance(pkt, IMUPacket):
            self.robot_state.update_imu(pkt.yaw, pkt.pitch, pkt.roll)
        elif isinstance(pkt, ProximityPacket):
            self.robot_state.update_proximity(pkt.values)
        elif isinstance(pkt, MetalDetectPacket):
            self.robot_state.update_metal(pkt.detected)
        elif isinstance(pkt, LiftStatePacket):
            self.robot_state.update_lift_state(pkt.state_str, pkt.magnet_mask)
        elif isinstance(pkt, UltrasonicPacket):
            self.robot_state.update_ultrasonic(pkt.distance_cm)
        elif isinstance(pkt, DiagnosticsPacket):
            self.robot_state.update_diagnostics(pkt.loop_hz, pkt.cpu_percent, pkt.fault_code)
        elif isinstance(pkt, StatusPacket):
            self.robot_state.set_status_message(pkt.message)
        elif isinstance(pkt, ErrorPacket):
            self.robot_state.set_error_message(f"E{pkt.code}: {pkt.message}")

        self.telemetry_updated.emit()

    def _on_safety_state_changed(self, safety_state: SafetyState) -> None:
        self.safety_status_changed.emit(safety_state.value)

    def _on_periodic_tick(self) -> None:
        """Periodic safety and command heartbeat handler."""
        # Check if telemetry is stale
        self.safety.check_staleness()
        # Stream drive commands if robot is moving
        self.command_manager.update_heartbeat()

    # --- UI Actions ---

    def connect_serial(self, port: str, baud: int) -> None:
        self.robot_state.selected_port = port
        self.robot_state.active_baud = baud
        self._worker.connect_port(port, baud)

    def disconnect_serial(self) -> None:
        # Best-effort neutral stop before disconnecting
        self.command_manager.request_stop()
        self.safety.disarm()
        self._worker.disconnect_port()

    def set_simulation_mode(self, enabled: bool) -> None:
        self.simulation_mode = enabled
        self._worker.simulation_mode = enabled

    def trigger_emergency_stop(self) -> None:
        self.command_manager.trigger_estop()

    def reset_estop(self) -> None:
        self.safety.reset_estop_latch()
        # Also send CCLEAR to Arduino
        self.command_manager.send_extended_command(self.protocol.encode_clear())

    def arm_robot(self) -> bool:
        return self.safety.arm()

    def disarm_robot(self) -> None:
        self.command_manager.request_stop()
        self.safety.disarm()

    def drive_wheels(
        self,
        right_vel: float,
        left_vel: float,
        gripper_vel: Optional[float] = None,
        servo_angle: Optional[float] = None,
        immediate: bool = False,
    ) -> bool:
        return self.command_manager.set_drive_targets(
            right_vel, left_vel, gripper_vel, servo_angle, immediate=immediate
        )

    def stop_movement(self) -> None:
        self.command_manager.request_stop()

    def trigger_lift_cycle(self) -> None:
        self.command_manager.send_extended_command(self.protocol.encode_lift_cycle())

    def lower_lift(self) -> None:
        self.command_manager.send_extended_command(self.protocol.encode_lift_lower())

    def stop_lift(self) -> None:
        self.command_manager.send_extended_command(self.protocol.encode_lift_stop())

    def set_magnets(self, energize: bool) -> None:
        self.command_manager.send_extended_command(self.protocol.encode_magnets(energize))

    def set_siren(self, activate: bool) -> None:
        self.command_manager.send_extended_command(self.protocol.encode_siren(activate))

    def set_servo_angle(self, angle_deg: int) -> None:
        self.command_manager.send_extended_command(self.protocol.encode_servo(angle_deg))

    def set_pid(self, enable: bool) -> None:
        self.command_manager.send_extended_command(self.protocol.encode_pid(enable))

    def reset_odometry(self) -> None:
        self.command_manager.send_extended_command(self.protocol.encode_reset())

    def shutdown(self) -> None:
        """Clean shutdown of threads and timers."""
        self._tick_timer.stop()
        if self._worker:
            self._worker.stop()
        if self._thread and self._thread.isRunning():
            self._thread.quit()
            self._thread.wait(1000)
        logger.info("ApplicationController shutdown complete.")
