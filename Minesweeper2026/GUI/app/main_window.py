"""
Main Window: Primary desktop GUI for Minesweeper Robot Control Station.
Assembles all panels into a responsive, engineering-oriented workspace with dark theme.
Handles keyboard teleoperation, window focus events, and safe shutdown.
"""

from datetime import datetime
import logging
from typing import Optional, Set
from PySide6.QtCore import QEvent, Qt, Slot
from PySide6.QtGui import QCloseEvent, QFont, QIcon, QKeySequence, QShortcut
from PySide6.QtWidgets import (
    QApplication,
    QFrame,
    QGridLayout,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QMainWindow,
    QMessageBox,
    QPlainTextEdit,
    QScrollArea,
    QSplitter,
    QStatusBar,
    QTabWidget,
    QVBoxLayout,
    QWidget,
)

from app.application_controller import ApplicationController
from app.settings_manager import SettingsManager
from ui.connection_panel import ConnectionPanel
from ui.mechanism_panel import MechanismPanel
from ui.motor_test_panel import MotorTestPanel
from ui.movement_panel import MovementPanel
from ui.safety_panel import SafetyPanel
from ui.serial_console import SerialConsole
from ui.speed_panel import SpeedPanel
from ui.telemetry_panel import TelemetryPanel

logger = logging.getLogger("ControlStation.MainWindow")


class MainWindow(QMainWindow):
    """Main application window."""

    def __init__(self, controller: ApplicationController, settings: SettingsManager):
        super().__init__()
        self.controller = controller
        self.settings = settings
        self._held_keys: Set[int] = set()

        self.setWindowTitle("Minesweeper Robot Control Station — Arduino Mega 2560")
        self.resize(
            self.settings.get("ui", "window_width", 1280),
            self.settings.get("ui", "window_height", 820),
        )

        self._apply_theme()
        self._init_ui()
        self._connect_signals()
        self._init_shortcuts()

    def _apply_theme(self) -> None:
        """Apply dark engineering stylesheet."""
        self.setStyleSheet(
            """
            QMainWindow, QWidget {
                background-color: #1a1e24;
                color: #e0e6ed;
                font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
            }
            QGroupBox {
                border: 1px solid #2e3846;
                border-radius: 6px;
                margin-top: 12px;
                font-weight: bold;
                font-size: 12px;
                color: #90caf9;
                padding-top: 10px;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                subcontrol-position: top left;
                padding: 0 6px;
                background-color: #1a1e24;
            }
            QComboBox, QSpinBox, QDoubleSpinBox, QLineEdit {
                background-color: #262f3d;
                border: 1px solid #3d4d63;
                border-radius: 4px;
                padding: 4px 8px;
                color: #ffffff;
            }
            QComboBox:hover, QSpinBox:hover, QDoubleSpinBox:hover {
                border-color: #00bcd4;
            }
            QPushButton {
                background-color: #2e3846;
                border: 1px solid #3d4d63;
                border-radius: 4px;
                padding: 6px 12px;
                color: #ffffff;
                font-weight: 500;
            }
            QPushButton:hover {
                background-color: #3d4d63;
                border-color: #00bcd4;
            }
            QPushButton:pressed {
                background-color: #00838f;
            }
            QTabWidget::pane {
                border: 1px solid #2e3846;
                border-radius: 4px;
                background-color: #1a1e24;
            }
            QTabBar::tab {
                background-color: #262f3d;
                color: #90a4ae;
                padding: 8px 16px;
                border-top-left-radius: 4px;
                border-top-right-radius: 4px;
                font-weight: bold;
            }
            QTabBar::tab:selected {
                background-color: #1a1e24;
                color: #00e5ff;
                border-bottom: 2px solid #00e5ff;
            }
            QStatusBar {
                background-color: #121519;
                color: #78909c;
                font-size: 11px;
                border-top: 1px solid #2e3846;
            }
            QSlider::groove:horizontal {
                height: 6px;
                background: #2e3846;
                border-radius: 3px;
            }
            QSlider::sub-page:horizontal {
                background: #00bcd4;
                border-radius: 3px;
            }
            QSlider::handle:horizontal {
                background: #e0f7fa;
                border: 1px solid #00838f;
                width: 14px;
                margin-top: -4px;
                margin-bottom: -4px;
                border-radius: 7px;
            }
            QScrollArea {
                border: none;
                background-color: transparent;
            }
            QScrollBar:vertical {
                border: none;
                background: #1a1e24;
                width: 8px;
                margin: 0px;
            }
            QScrollBar::handle:vertical {
                background: #37474f;
                min-height: 20px;
                border-radius: 4px;
            }
            QScrollBar::handle:vertical:hover {
                background: #00bcd4;
            }
            QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
                height: 0px;
            }
            """
        )

    def _init_ui(self) -> None:
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        main_layout = QVBoxLayout(central_widget)
        main_layout.setContentsMargins(10, 10, 10, 6)
        main_layout.setSpacing(8)

        # Top Bar: Connection & Safety Interlocks (High Visibility)
        top_bar = QHBoxLayout()
        self.panel_conn = ConnectionPanel()
        self.panel_safety = SafetyPanel()
        top_bar.addWidget(self.panel_conn, stretch=3)
        top_bar.addWidget(self.panel_safety, stretch=4)
        main_layout.addLayout(top_bar)

        # Main Splitter (Left: Controls, Right: Telemetry & Console)
        splitter = QSplitter(Qt.Horizontal)

        # LEFT WORKSPACE (Drive & Mechanisms)
        left_widget = QWidget()
        left_layout = QVBoxLayout(left_widget)
        left_layout.setContentsMargins(0, 0, 0, 0)
        left_layout.setSpacing(8)

        self.panel_movement = MovementPanel()
        self.panel_speed = SpeedPanel(
            max_speed=self.settings.get("limits", "max_velocity_rad_s", 10.0),
            default_speed=self.settings.get("limits", "default_velocity_rad_s", 3.0),
        )

        left_layout.addWidget(self.panel_movement)
        left_layout.addWidget(self.panel_speed)

        self.tab_widget = QTabWidget()
        self.panel_mech = MechanismPanel()
        self.panel_motor_test = MotorTestPanel()
        self.tab_widget.addTab(self.panel_mech, "Auxiliary Mechanisms")
        self.tab_widget.addTab(self.panel_motor_test, "Independent Motor Test")
        left_layout.addWidget(self.tab_widget)
        # Wrap Left Workspace in a QScrollArea so panels never squash or overflow on smaller laptop screens
        left_scroll = QScrollArea()
        left_scroll.setWidgetResizable(True)
        left_scroll.setFrameShape(QFrame.NoFrame)
        left_scroll.setHorizontalScrollBarPolicy(Qt.ScrollBarAsNeeded)
        left_scroll.setVerticalScrollBarPolicy(Qt.ScrollBarAsNeeded)
        left_scroll.setWidget(left_widget)

        splitter.addWidget(left_scroll)

        # RIGHT WORKSPACE (Telemetry Dashboard & Terminal)
        right_widget = QWidget()
        right_layout = QVBoxLayout(right_widget)
        right_layout.setContentsMargins(0, 0, 0, 0)
        right_layout.setSpacing(8)

        self.panel_telemetry = TelemetryPanel()
        self.panel_console = SerialConsole(
            max_lines=self.settings.get("ui", "max_console_lines", 1000)
        )

        right_layout.addWidget(self.panel_telemetry, stretch=3)
        right_layout.addWidget(self.panel_console, stretch=4)

        splitter.addWidget(right_widget)
        splitter.setStretchFactor(0, 4)
        splitter.setStretchFactor(1, 5)

        main_layout.addWidget(splitter)

        # Status Bar
        self.status_bar = QStatusBar()
        self.setStatusBar(self.status_bar)

        self.lbl_status_link = QLabel("Link: Disconnected")
        self.lbl_status_safety = QLabel("Safety: DISARMED")
        self.lbl_status_last_rx = QLabel("Last RX: --:--:--")
        self.lbl_status_latency = QLabel("Watchdog: OK")

        self.status_bar.addWidget(self.lbl_status_link, 2)
        self.status_bar.addWidget(self.lbl_status_safety, 2)
        self.status_bar.addWidget(self.lbl_status_last_rx, 2)
        self.status_bar.addWidget(self.lbl_status_latency, 2)

    def _connect_signals(self) -> None:
        """Connect UI components to ApplicationController."""
        # Connection
        self.panel_conn.connect_requested.connect(self.controller.connect_serial)
        self.panel_conn.disconnect_requested.connect(self.controller.disconnect_serial)
        self.panel_conn.simulation_toggled.connect(self.controller.set_simulation_mode)

        # Controller -> UI
        self.controller.connection_status_changed.connect(self.panel_conn.set_status)
        self.controller.connection_status_changed.connect(self._on_connection_status_changed)
        self.controller.safety_status_changed.connect(self.panel_safety.update_safety_state)
        self.controller.safety_status_changed.connect(
            lambda st: self.lbl_status_safety.setText(f"Safety: {st}")
        )
        self.controller.telemetry_updated.connect(self._on_telemetry_updated)
        self.controller.console_message_received.connect(self.panel_console.append_message)
        self.controller.error_message_raised.connect(self._on_error_message)

        # Safety Panel
        self.panel_safety.estop_requested.connect(self.controller.trigger_emergency_stop)
        self.panel_safety.reset_estop_requested.connect(self.controller.reset_estop)
        self.panel_safety.arm_toggled.connect(self._on_arm_toggled)

        # Movement Panel (Mouse Hold / Click)
        self.panel_movement.direction_pressed.connect(self._on_direction_pressed)
        self.panel_movement.movement_released.connect(self._on_movement_released)

        # Motor Test Panel
        self.panel_motor_test.wheel_test_command.connect(self._on_wheel_test_command)
        self.panel_motor_test.stop_requested.connect(self.controller.stop_movement)

        # Mechanism Panel
        self.panel_mech.lift_cycle_requested.connect(self.controller.trigger_lift_cycle)
        self.panel_mech.lift_lower_requested.connect(self.controller.lower_lift)
        self.panel_mech.lift_stop_requested.connect(self.controller.stop_lift)
        self.panel_mech.magnets_toggled.connect(self.controller.set_magnets)
        self.panel_mech.siren_toggled.connect(self.controller.set_siren)
        self.panel_mech.servo_angle_changed.connect(self.controller.set_servo_angle)
        self.panel_mech.pid_toggled.connect(self.controller.set_pid)
        self.panel_mech.reset_odometry_requested.connect(self.controller.reset_odometry)

    def _init_shortcuts(self) -> None:
        """Global safety shortcuts."""
        # Space & Esc -> Immediate Emergency Stop
        self.shortcut_space = QShortcut(QKeySequence(Qt.Key_Space), self)
        self.shortcut_space.activated.connect(self.controller.trigger_emergency_stop)

        self.shortcut_esc = QShortcut(QKeySequence(Qt.Key_Escape), self)
        self.shortcut_esc.activated.connect(self.controller.trigger_emergency_stop)

    # --- Keyboard Teleoperation ---

    def keyPressEvent(self, event) -> None:
        """Handle W, S, A, D keyboard movement inputs."""
        # Ignore if user is typing in a text field
        focus_widget = QApplication.focusWidget()
        if focus_widget and isinstance(focus_widget, (QLineEdit, QPlainTextEdit)):
            super().keyPressEvent(event)
            return

        key = event.key()
        if event.isAutoRepeat():
            return

        if key in (Qt.Key_W, Qt.Key_S, Qt.Key_A, Qt.Key_D):
            self._held_keys.add(key)
            self._evaluate_keyboard_movement()
        else:
            super().keyPressEvent(event)

    def keyReleaseEvent(self, event) -> None:
        """Handle key release and stop robot when keys are released."""
        if event.isAutoRepeat():
            return

        key = event.key()
        if key in self._held_keys:
            self._held_keys.remove(key)
            self._evaluate_keyboard_movement()
        else:
            super().keyReleaseEvent(event)

    def changeEvent(self, event: QEvent) -> None:
        """Clear held keys and stop robot immediately if window loses focus."""
        if event.type() == QEvent.ActivationChange:
            if not self.isActiveWindow():
                if self._held_keys:
                    logger.info("Window focus lost; releasing held movement keys.")
                    self._held_keys.clear()
                    self.controller.stop_movement()
                    self.panel_movement.set_active_display(None)
        super().changeEvent(event)

    def _evaluate_keyboard_movement(self) -> None:
        """Calculate differential drive velocities from active held keys."""
        if not self._held_keys:
            self.controller.stop_movement()
            self.panel_movement.set_active_display(None)
            return

        lin_speed = self.panel_speed.get_linear_speed()
        turn_speed = self.panel_speed.get_turn_speed()

        r_vel = 0.0
        l_vel = 0.0
        direction_name: Optional[str] = None

        if Qt.Key_W in self._held_keys and Qt.Key_S not in self._held_keys:
            r_vel = lin_speed
            l_vel = lin_speed
            direction_name = "FORWARD"
        elif Qt.Key_S in self._held_keys and Qt.Key_W not in self._held_keys:
            r_vel = -lin_speed
            l_vel = -lin_speed
            direction_name = "BACKWARD"

        if Qt.Key_A in self._held_keys and Qt.Key_D not in self._held_keys:
            # Turn left: right wheel forward, left wheel reverse
            r_vel = turn_speed
            l_vel = -turn_speed
            direction_name = "TURN LEFT"
        elif Qt.Key_D in self._held_keys and Qt.Key_A not in self._held_keys:
            # Turn right: right wheel reverse, left wheel forward
            r_vel = -turn_speed
            l_vel = turn_speed
            direction_name = "TURN RIGHT"

        if direction_name:
            if not self.controller.safety.is_armed and not self.controller.safety.is_estop_latched:
                self.controller.arm_robot()
            self.panel_movement.set_active_display(direction_name)
            self.controller.drive_wheels(r_vel, l_vel, immediate=True)
        else:
            self.panel_movement.set_active_display(None)
            self.controller.stop_movement()

    def _on_direction_pressed(self, direction: str) -> None:
        """Handle directional button press from GUI pad."""
        lin = self.panel_speed.get_linear_speed()
        trn = self.panel_speed.get_turn_speed()

        if direction != "STOP":
            if not self.controller.safety.is_armed and not self.controller.safety.is_estop_latched:
                self.controller.arm_robot()

        if direction == "FORWARD":
            self.controller.drive_wheels(lin, lin, immediate=True)
        elif direction == "BACKWARD":
            self.controller.drive_wheels(-lin, -lin, immediate=True)
        elif direction == "LEFT":
            self.controller.drive_wheels(trn, -trn, immediate=True)
        elif direction == "RIGHT":
            self.controller.drive_wheels(-trn, trn, immediate=True)
        elif direction == "STOP":
            self.controller.stop_movement()

    def _on_movement_released(self) -> None:
        self.controller.stop_movement()

    def _on_wheel_test_command(self, r_vel: float, l_vel: float) -> None:
        self.controller.drive_wheels(r_vel, l_vel, immediate=True)

    def _on_arm_toggled(self, armed: bool) -> None:
        if armed:
            if not self.controller.arm_robot():
                QMessageBox.warning(
                    self,
                    "Arming Failed",
                    "Cannot arm robot. Ensure serial is connected and Emergency Stop is not latched.",
                )
        else:
            self.controller.disarm_robot()

    def _on_connection_status_changed(self, status: str, details: str) -> None:
        self.lbl_status_link.setText(f"Link: {status} ({details})")

    def _on_telemetry_updated(self) -> None:
        state = self.controller.robot_state
        self.panel_telemetry.update_telemetry(state)
        self.panel_mech.update_from_telemetry(state.lift.magnet_mask > 0)

        # Update last RX time in status bar
        if state.last_rx_time > 0:
            dt = datetime.fromtimestamp(state.last_rx_time).strftime("%H:%M:%S")
            self.lbl_status_last_rx.setText(f"Last RX: {dt}")

        if state.is_communication_stale():
            self.lbl_status_latency.setText("Watchdog: STALE (>1.5s)")
            self.lbl_status_latency.setStyleSheet("color: #ff5252; font-weight: bold;")
        else:
            self.lbl_status_latency.setText("Watchdog: OK (Live)")
            self.lbl_status_latency.setStyleSheet("color: #81c784;")

    def _on_error_message(self, err_text: str) -> None:
        self.panel_console.append_message("RX", f"E:ERROR: {err_text}")

    def closeEvent(self, event: QCloseEvent) -> None:
        """Handle window close event with safe stop and shutdown."""
        logger.info("Application closing. Performing safe shutdown...")
        # Save geometry
        self.settings.set("ui", "window_width", self.width())
        self.settings.set("ui", "window_height", self.height())
        self.settings.save()

        # Stop robot and terminate background thread
        self.controller.shutdown()
        event.accept()
