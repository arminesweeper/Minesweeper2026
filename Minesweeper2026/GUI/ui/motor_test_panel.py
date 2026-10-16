"""
Motor Test Panel: Independent wheel motor testing with safety arming gate.
Allows individual verification of left and right drive motors at safe test speeds.
"""

from PySide6.QtCore import Qt, Signal
from PySide6.QtWidgets import (
    QCheckBox,
    QDoubleSpinBox,
    QFrame,
    QGridLayout,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QPushButton,
    QVBoxLayout,
    QWidget,
)


class MotorTestPanel(QGroupBox):
    """Testing panel for individual left and right drive motors."""

    # Signals: (right_vel, left_vel)
    wheel_test_command = Signal(float, float)
    stop_requested = Signal()

    def __init__(self, parent: QWidget | None = None):
        super().__init__("Independent Motor Diagnostics", parent)
        self._init_ui()

    def _init_ui(self) -> None:
        layout = QVBoxLayout(self)
        layout.setSpacing(8)

        # Safety Checkbox
        self.arm_checkbox = QCheckBox("Enable Independent Wheel Test Mode")
        self.arm_checkbox.setStyleSheet("color: #ffb74d; font-weight: bold;")
        self.arm_checkbox.toggled.connect(self._on_arm_toggled)
        layout.addWidget(self.arm_checkbox)

        # Test Speed Row
        speed_row = QHBoxLayout()
        speed_row.addWidget(QLabel("Test Speed:"))
        self.spin_test_speed = QDoubleSpinBox()
        self.spin_test_speed.setRange(0.5, 3.0)
        self.spin_test_speed.setValue(1.5)
        self.spin_test_speed.setSingleStep(0.2)
        self.spin_test_speed.setSuffix(" rad/s")
        speed_row.addWidget(self.spin_test_speed)
        speed_row.addStretch()
        layout.addLayout(speed_row)

        # Warning
        self.lbl_warning = QLabel("Warning: Independent wheel drive will cause rotation!")
        self.lbl_warning.setStyleSheet("color: #ef5350; font-size: 10px; font-style: italic;")
        layout.addWidget(self.lbl_warning)

        # Test Grid
        self.test_container = QWidget()
        grid = QGridLayout(self.test_container)
        grid.setSpacing(6)

        btn_style = (
            "QPushButton {"
            "   background-color: #37474f;"
            "   color: white;"
            "   font-weight: bold;"
            "   padding: 6px;"
            "   border-radius: 4px;"
            "}"
            "QPushButton:hover { background-color: #455a64; }"
            "QPushButton:pressed { background-color: #00838f; }"
            "QPushButton:disabled { background-color: #263238; color: #546e7a; }"
        )

        # Left Motor Column
        grid.addWidget(QLabel("Left Wheel Motor:"), 0, 0)
        self.btn_left_fwd = QPushButton("Left FWD")
        self.btn_left_fwd.setStyleSheet(btn_style)
        self.btn_left_fwd.pressed.connect(self._on_left_fwd)
        self.btn_left_fwd.released.connect(self._on_release)
        grid.addWidget(self.btn_left_fwd, 1, 0)

        self.btn_left_rev = QPushButton("Left REV")
        self.btn_left_rev.setStyleSheet(btn_style)
        self.btn_left_rev.pressed.connect(self._on_left_rev)
        self.btn_left_rev.released.connect(self._on_release)
        grid.addWidget(self.btn_left_rev, 2, 0)

        # Right Motor Column
        grid.addWidget(QLabel("Right Wheel Motor:"), 0, 1)
        self.btn_right_fwd = QPushButton("Right FWD")
        self.btn_right_fwd.setStyleSheet(btn_style)
        self.btn_right_fwd.pressed.connect(self._on_right_fwd)
        self.btn_right_fwd.released.connect(self._on_release)
        grid.addWidget(self.btn_right_fwd, 1, 1)

        self.btn_right_rev = QPushButton("Right REV")
        self.btn_right_rev.setStyleSheet(btn_style)
        self.btn_right_rev.pressed.connect(self._on_right_rev)
        self.btn_right_rev.released.connect(self._on_release)
        grid.addWidget(self.btn_right_rev, 2, 1)

        layout.addWidget(self.test_container)
        self.test_container.setEnabled(False)

        # Stop All Button
        self.btn_stop_all = QPushButton("Stop Motors")
        self.btn_stop_all.setStyleSheet("background-color: #c62828; color: white; font-weight: bold; padding: 6px;")
        self.btn_stop_all.clicked.connect(self._on_stop_clicked)
        layout.addWidget(self.btn_stop_all)

    def _on_arm_toggled(self, checked: bool) -> None:
        self.test_container.setEnabled(checked)
        if not checked:
            self.stop_requested.emit()

    def _get_speed(self) -> float:
        return self.spin_test_speed.value()

    def _on_left_fwd(self) -> None:
        self.wheel_test_command.emit(0.0, self._get_speed())

    def _on_left_rev(self) -> None:
        self.wheel_test_command.emit(0.0, -self._get_speed())

    def _on_right_fwd(self) -> None:
        self.wheel_test_command.emit(self._get_speed(), 0.0)

    def _on_right_rev(self) -> None:
        self.wheel_test_command.emit(-self._get_speed(), 0.0)

    def _on_release(self) -> None:
        self.stop_requested.emit()

    def _on_stop_clicked(self) -> None:
        self.stop_requested.emit()
