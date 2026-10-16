"""
Mechanism Panel: Controls verified robot mechanisms.
Includes automated mine lift, electromagnets, 12V siren, camera servo, and PID mode.
Strictly maps to confirmed firmware commands in SystemManager.cpp.
"""

from PySide6.QtCore import Qt, Signal
from PySide6.QtWidgets import (
    QCheckBox,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QPushButton,
    QSlider,
    QVBoxLayout,
    QWidget,
)


class MechanismPanel(QGroupBox):
    """UI Panel for auxiliary robot mechanisms (Lift, Magnets, Siren, Servo, PID)."""

    lift_cycle_requested = Signal()
    lift_lower_requested = Signal()
    lift_stop_requested = Signal()
    magnets_toggled = Signal(bool)
    siren_toggled = Signal(bool)
    servo_angle_changed = Signal(int)
    pid_toggled = Signal(bool)
    reset_odometry_requested = Signal()

    def __init__(self, parent: QWidget | None = None):
        super().__init__("Robot Mechanism Controls", parent)
        self._magnets_on = False
        self._siren_on = False
        self._init_ui()

    def _init_ui(self) -> None:
        layout = QVBoxLayout(self)
        layout.setSpacing(10)

        # 1. Mine Lift Mechanism Controls
        lift_group = QGroupBox("Mine Lift Actuator (J37 / J44)")
        lift_layout = QHBoxLayout(lift_group)

        self.btn_lift_cycle = QPushButton("Cycle Pickup")
        self.btn_lift_cycle.setToolTip("Energizes magnets, raises lift to top, dwells to drop, and lowers (CLIFT:CYCLE)")
        self.btn_lift_cycle.setStyleSheet("background-color: #00796b; color: white; font-weight: bold; padding: 6px;")
        self.btn_lift_cycle.clicked.connect(self.lift_cycle_requested.emit)
        lift_layout.addWidget(self.btn_lift_cycle)

        self.btn_lift_lower = QPushButton("Lower to Bottom")
        self.btn_lift_lower.setToolTip("Lowers lift until bottom limit switch triggers (CLIFT:DN)")
        self.btn_lift_lower.setStyleSheet("background-color: #00695c; color: white; padding: 6px;")
        self.btn_lift_lower.clicked.connect(self.lift_lower_requested.emit)
        lift_layout.addWidget(self.btn_lift_lower)

        self.btn_lift_stop = QPushButton("Stop Lift")
        self.btn_lift_stop.setToolTip("Immediately cuts lift motor PWM (CLIFT:STOP)")
        self.btn_lift_stop.setStyleSheet("background-color: #b71c1c; color: white; padding: 6px;")
        self.btn_lift_stop.clicked.connect(self.lift_stop_requested.emit)
        lift_layout.addWidget(self.btn_lift_stop)

        layout.addWidget(lift_group)

        # 2. Electromagnets & Siren Relay
        relay_row = QHBoxLayout()

        self.btn_magnets = QPushButton("Magnets: OFF")
        self.btn_magnets.setStyleSheet("background-color: #37474f; color: white; font-weight: bold; padding: 7px;")
        self.btn_magnets.clicked.connect(self._toggle_magnets)
        relay_row.addWidget(self.btn_magnets)

        self.btn_siren = QPushButton("Siren: SILENT")
        self.btn_siren.setStyleSheet("background-color: #37474f; color: white; font-weight: bold; padding: 7px;")
        self.btn_siren.clicked.connect(self._toggle_siren)
        relay_row.addWidget(self.btn_siren)

        layout.addLayout(relay_row)

        # 3. Camera Pan Servo
        servo_group = QGroupBox("Camera Pan Servo (Pin 4)")
        servo_layout = QVBoxLayout(servo_group)

        slider_row = QHBoxLayout()
        slider_row.addWidget(QLabel("Angle (0°-180°):"))
        self.servo_slider = QSlider(Qt.Horizontal)
        self.servo_slider.setRange(0, 180)
        self.servo_slider.setValue(90)
        self.servo_slider.valueChanged.connect(self._on_servo_slider_changed)
        slider_row.addWidget(self.servo_slider)

        self.lbl_servo_val = QLabel("90°")
        self.lbl_servo_val.setMinimumWidth(35)
        slider_row.addWidget(self.lbl_servo_val)
        servo_layout.addLayout(slider_row)

        presets_row = QHBoxLayout()
        presets = [("Left (0°)", 0), ("Center (90°)", 90), ("Right (180°)", 180)]
        for lbl, ang in presets:
            btn = QPushButton(lbl)
            btn.setStyleSheet("font-size: 11px; padding: 3px;")
            btn.clicked.connect(lambda _, a=ang: self.servo_slider.setValue(a))
            presets_row.addWidget(btn)
        servo_layout.addLayout(presets_row)

        layout.addWidget(servo_group)

        # 4. PID Closed-Loop & Odometry Reset
        bottom_row = QHBoxLayout()

        self.check_pid = QCheckBox("Closed-Loop PID")
        self.check_pid.setChecked(True)
        self.check_pid.setToolTip("Checked = PID velocity control; Unchecked = Direct open-loop PWM")
        self.check_pid.toggled.connect(self.pid_toggled.emit)
        bottom_row.addWidget(self.check_pid)

        bottom_row.addStretch()

        self.btn_reset_odom = QPushButton("Reset Odometry & IMU")
        self.btn_reset_odom.setToolTip("Resets X, Y, Theta and zero-offsets IMU yaw (CRESET)")
        self.btn_reset_odom.setStyleSheet("background-color: #455a64; color: white; padding: 5px;")
        self.btn_reset_odom.clicked.connect(self.reset_odometry_requested.emit)
        bottom_row.addWidget(self.btn_reset_odom)

        layout.addLayout(bottom_row)

    def _toggle_magnets(self) -> None:
        self._magnets_on = not self._magnets_on
        if self._magnets_on:
            self.btn_magnets.setText("Magnets: ON (Energized)")
            self.btn_magnets.setStyleSheet("background-color: #2e7d32; color: white; font-weight: bold; padding: 7px;")
        else:
            self.btn_magnets.setText("Magnets: OFF")
            self.btn_magnets.setStyleSheet("background-color: #37474f; color: white; font-weight: bold; padding: 7px;")
        self.magnets_toggled.emit(self._magnets_on)

    def _toggle_siren(self) -> None:
        self._siren_on = not self._siren_on
        if self._siren_on:
            self.btn_siren.setText("Siren: ALERTING!")
            self.btn_siren.setStyleSheet("background-color: #d32f2f; color: white; font-weight: bold; padding: 7px;")
        else:
            self.btn_siren.setText("Siren: SILENT")
            self.btn_siren.setStyleSheet("background-color: #37474f; color: white; font-weight: bold; padding: 7px;")
        self.siren_toggled.emit(self._siren_on)

    def _on_servo_slider_changed(self, val: int) -> None:
        self.lbl_servo_val.setText(f"{val}°")
        self.servo_angle_changed.emit(val)

    def update_from_telemetry(self, magnets_active: bool) -> None:
        """Update magnet button appearance if firmware reports state."""
        if magnets_active != self._magnets_on:
            self._magnets_on = magnets_active
            if self._magnets_on:
                self.btn_magnets.setText("Magnets: ON (Energized)")
                self.btn_magnets.setStyleSheet("background-color: #2e7d32; color: white; font-weight: bold; padding: 7px;")
            else:
                self.btn_magnets.setText("Magnets: OFF")
                self.btn_magnets.setStyleSheet("background-color: #37474f; color: white; font-weight: bold; padding: 7px;")
