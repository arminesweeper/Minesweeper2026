"""
Movement Panel: Directional control pad with press-and-hold behavior.
Sends movement commands while pressed and neutral stop upon release.
"""

from typing import Optional
from PySide6.QtCore import Qt, Signal
from PySide6.QtWidgets import (
    QGridLayout,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QPushButton,
    QVBoxLayout,
    QWidget,
)


class HoldButton(QPushButton):
    """Button that emits separate pressed and released signals for continuous drive."""

    def __init__(self, text: str, parent: QWidget | None = None):
        super().__init__(text, parent)
        self.setFocusPolicy(Qt.NoFocus)  # Prevent stealing keyboard focus from main window


class MovementPanel(QGroupBox):
    """Directional movement control pad."""

    # Signals: direction ("FORWARD", "BACKWARD", "LEFT", "RIGHT", "STOP")
    direction_pressed = Signal(str)
    movement_released = Signal()

    def __init__(self, parent: QWidget | None = None):
        super().__init__("Drive Control Pad", parent)
        self._active_direction: Optional[str] = None
        self._init_ui()

    def _init_ui(self) -> None:
        layout = QVBoxLayout(self)
        layout.setSpacing(10)

        grid = QGridLayout()
        grid.setSpacing(8)

        btn_style = (
            "QPushButton {"
            "   background-color: #263238;"
            "   color: #eceff1;"
            "   font-size: 14px;"
            "   font-weight: bold;"
            "   border: 2px solid #455a64;"
            "   border-radius: 6px;"
            "   min-height: 48px;"
            "}"
            "QPushButton:hover { background-color: #37474f; border-color: #78909c; }"
            "QPushButton:pressed { background-color: #00838f; border-color: #4dd0e1; color: white; }"
        )

        stop_style = (
            "QPushButton {"
            "   background-color: #c62828;"
            "   color: white;"
            "   font-size: 14px;"
            "   font-weight: 900;"
            "   border: 2px solid #b71c1c;"
            "   border-radius: 6px;"
            "   min-height: 48px;"
            "}"
            "QPushButton:hover { background-color: #d32f2f; }"
            "QPushButton:pressed { background-color: #8e0000; }"
        )

        # Forward (Row 0, Col 1)
        self.btn_fwd = HoldButton("▲ FORWARD (W)")
        self.btn_fwd.setStyleSheet(btn_style)
        self.btn_fwd.pressed.connect(lambda: self._on_btn_press("FORWARD"))
        self.btn_fwd.released.connect(self._on_btn_release)
        grid.addWidget(self.btn_fwd, 0, 1)

        # Left (Row 1, Col 0)
        self.btn_left = HoldButton("◄ LEFT (A)")
        self.btn_left.setStyleSheet(btn_style)
        self.btn_left.pressed.connect(lambda: self._on_btn_press("LEFT"))
        self.btn_left.released.connect(self._on_btn_release)
        grid.addWidget(self.btn_left, 1, 0)

        # Stop (Row 1, Col 1)
        self.btn_stop = HoldButton("■ STOP")
        self.btn_stop.setStyleSheet(stop_style)
        self.btn_stop.clicked.connect(self._on_stop_clicked)
        grid.addWidget(self.btn_stop, 1, 1)

        # Right (Row 1, Col 2)
        self.btn_right = HoldButton("RIGHT (D) ►")
        self.btn_right.setStyleSheet(btn_style)
        self.btn_right.pressed.connect(lambda: self._on_btn_press("RIGHT"))
        self.btn_right.released.connect(self._on_btn_release)
        grid.addWidget(self.btn_right, 1, 2)

        # Backward (Row 2, Col 1)
        self.btn_bwd = HoldButton("▼ BACKWARD (S)")
        self.btn_bwd.setStyleSheet(btn_style)
        self.btn_bwd.pressed.connect(lambda: self._on_btn_press("BACKWARD"))
        self.btn_bwd.released.connect(self._on_btn_release)
        grid.addWidget(self.btn_bwd, 2, 1)

        layout.addLayout(grid)

        # Active Command Indicator
        self.lbl_active = QLabel("Active Command: NEUTRAL STOP")
        self.lbl_active.setAlignment(Qt.AlignCenter)
        self.lbl_active.setStyleSheet(
            "color: #78909c; font-size: 11px; font-weight: bold; padding: 4px;"
        )
        layout.addWidget(self.lbl_active)

    def _on_btn_press(self, direction: str) -> None:
        self._active_direction = direction
        self.lbl_active.setText(f"Active Command: {direction}")
        self.lbl_active.setStyleSheet("color: #4fc3f7; font-size: 11px; font-weight: bold; padding: 4px;")
        self.direction_pressed.emit(direction)

    def _on_btn_release(self) -> None:
        self._active_direction = None
        self.lbl_active.setText("Active Command: NEUTRAL STOP")
        self.lbl_active.setStyleSheet("color: #78909c; font-size: 11px; font-weight: bold; padding: 4px;")
        self.movement_released.emit()

    def _on_stop_clicked(self) -> None:
        self._active_direction = None
        self.lbl_active.setText("Active Command: STOPPED")
        self.lbl_active.setStyleSheet("color: #ff5252; font-size: 11px; font-weight: bold; padding: 4px;")
        self.movement_released.emit()

    def set_active_display(self, direction: Optional[str]) -> None:
        """Called by main window when keyboard drives the robot."""
        self._active_direction = direction
        if direction:
            self.lbl_active.setText(f"Active Command: {direction} (Keyboard)")
            self.lbl_active.setStyleSheet("color: #4fc3f7; font-size: 11px; font-weight: bold; padding: 4px;")
        else:
            self.lbl_active.setText("Active Command: NEUTRAL STOP")
            self.lbl_active.setStyleSheet("color: #78909c; font-size: 11px; font-weight: bold; padding: 4px;")
