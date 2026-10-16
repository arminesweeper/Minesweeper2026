"""
Safety Panel: Emergency Stop, Arm/Disarm interlock, and safety state monitoring.
"""

from PySide6.QtCore import Qt, Signal
from PySide6.QtWidgets import (
    QFrame,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QPushButton,
    QVBoxLayout,
    QWidget,
)


class SafetyPanel(QGroupBox):
    """UI component for safety interlocking and emergency stop latching."""

    estop_requested = Signal()
    reset_estop_requested = Signal()
    arm_toggled = Signal(bool)

    def __init__(self, parent: QWidget | None = None):
        super().__init__("Safety & Emergency Stop", parent)
        self._is_armed = False
        self._is_latched = False
        self._init_ui()

    def _init_ui(self) -> None:
        layout = QVBoxLayout(self)
        layout.setSpacing(10)

        # Huge Emergency Stop Button
        self.btn_estop = QPushButton("EMERGENCY STOP (Space / Esc)")
        self.btn_estop.setMinimumHeight(55)
        self.btn_estop.setStyleSheet(
            "QPushButton {"
            "   background-color: #d32f2f;"
            "   color: white;"
            "   font-size: 16px;"
            "   font-weight: 900;"
            "   border: 3px solid #b71c1c;"
            "   border-radius: 8px;"
            "}"
            "QPushButton:hover { background-color: #f44336; }"
            "QPushButton:pressed { background-color: #9a0007; }"
        )
        self.btn_estop.clicked.connect(self._on_estop_clicked)
        layout.addWidget(self.btn_estop)

        # Row: Safety Status Badge and Arm/Disarm Button
        row = QHBoxLayout()

        self.status_badge = QLabel("STATE: DISARMED")
        self.status_badge.setAlignment(Qt.AlignCenter)
        self.status_badge.setStyleSheet(
            "QLabel {"
            "   background-color: #455a64;"
            "   color: white;"
            "   font-size: 13px;"
            "   font-weight: bold;"
            "   padding: 8px;"
            "   border-radius: 4px;"
            "}"
        )
        row.addWidget(self.status_badge, stretch=2)

        self.btn_arm = QPushButton("ARM ROBOT")
        self.btn_arm.setMinimumHeight(36)
        self.btn_arm.setStyleSheet(
            "QPushButton {"
            "   background-color: #388e3c;"
            "   color: white;"
            "   font-size: 13px;"
            "   font-weight: bold;"
            "   border-radius: 4px;"
            "}"
            "QPushButton:hover { background-color: #4caf50; }"
        )
        self.btn_arm.clicked.connect(self._on_arm_clicked)
        row.addWidget(self.btn_arm, stretch=1)

        self.btn_reset_latch = QPushButton("Reset E-Stop")
        self.btn_reset_latch.setEnabled(False)
        self.btn_reset_latch.setMinimumHeight(36)
        self.btn_reset_latch.setStyleSheet(
            "QPushButton {"
            "   background-color: #f57c00;"
            "   color: white;"
            "   font-size: 12px;"
            "   font-weight: bold;"
            "   border-radius: 4px;"
            "}"
            "QPushButton:disabled { background-color: #555555; color: #888888; }"
        )
        self.btn_reset_latch.clicked.connect(self._on_reset_latch_clicked)
        row.addWidget(self.btn_reset_latch, stretch=1)

        layout.addLayout(row)

        # Software Safety Notice
        notice = QLabel("Notice: Software E-Stop stops motors over serial. Hardware cutoff required for true safety.")
        notice.setStyleSheet("color: #90a4ae; font-size: 10px; font-style: italic;")
        notice.setAlignment(Qt.AlignCenter)
        layout.addWidget(notice)

    def _on_estop_clicked(self) -> None:
        self.estop_requested.emit()

    def _on_arm_clicked(self) -> None:
        if self._is_armed:
            self.arm_toggled.emit(False)
        else:
            self.arm_toggled.emit(True)

    def _on_reset_latch_clicked(self) -> None:
        self.reset_estop_requested.emit()

    def update_safety_state(self, state_str: str) -> None:
        """Update visual state and enable/disable arming controls."""
        if "ESTOP" in state_str:
            self._is_latched = True
            self._is_armed = False
            self.status_badge.setText("STATE: EMERGENCY STOP (LATCHED)")
            self.status_badge.setStyleSheet(
                "QLabel { background-color: #b71c1c; color: white; font-weight: bold; padding: 8px; border-radius: 4px; }"
            )
            self.btn_arm.setEnabled(False)
            self.btn_arm.setText("ARM ROBOT")
            self.btn_reset_latch.setEnabled(True)
        elif "ARMED" in state_str:
            self._is_latched = False
            self._is_armed = True
            self.status_badge.setText("STATE: ARMED (MOTORS ACTIVE)")
            self.status_badge.setStyleSheet(
                "QLabel { background-color: #1b5e20; color: #a5d6a7; font-weight: bold; padding: 8px; border-radius: 4px; }"
            )
            self.btn_arm.setEnabled(True)
            self.btn_arm.setText("DISARM")
            self.btn_arm.setStyleSheet("QPushButton { background-color: #d32f2f; color: white; font-weight: bold; }")
            self.btn_reset_latch.setEnabled(False)
        elif "FAULT" in state_str:
            self._is_armed = False
            self.status_badge.setText("STATE: SYSTEM FAULT")
            self.status_badge.setStyleSheet(
                "QLabel { background-color: #e65100; color: white; font-weight: bold; padding: 8px; border-radius: 4px; }"
            )
            self.btn_arm.setEnabled(False)
            self.btn_reset_latch.setEnabled(True)
        else:
            # DISARMED
            self._is_latched = False
            self._is_armed = False
            self.status_badge.setText("STATE: DISARMED")
            self.status_badge.setStyleSheet(
                "QLabel { background-color: #37474f; color: #cfd8dc; font-weight: bold; padding: 8px; border-radius: 4px; }"
            )
            self.btn_arm.setEnabled(True)
            self.btn_arm.setText("ARM ROBOT")
            self.btn_arm.setStyleSheet("QPushButton { background-color: #2e7d32; color: white; font-weight: bold; }")
            self.btn_reset_latch.setEnabled(False)
