"""
Speed Panel: Angular wheel velocity slider, spinbox, and preset configurations.
Enforces the firmware maximum wheel speed limit (10.0 rad/s).
"""

from PySide6.QtCore import Qt, Signal
from PySide6.QtWidgets import (
    QDoubleSpinBox,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QPushButton,
    QSlider,
    QVBoxLayout,
    QWidget,
)


class SpeedPanel(QGroupBox):
    """Controls robot target movement velocity within firmware limits."""

    speed_changed = Signal(float)  # target velocity in rad/s

    def __init__(self, max_speed: float = 10.0, default_speed: float = 3.0, parent: QWidget | None = None):
        super().__init__("Drive Speed Settings", parent)
        self.max_speed = max_speed
        self.default_speed = default_speed
        self._init_ui()

    def _init_ui(self) -> None:
        layout = QVBoxLayout(self)
        layout.setSpacing(8)

        # Row 1: Target Speed Slider and Numeric Spinbox
        row1 = QHBoxLayout()
        row1.addWidget(QLabel("Speed (rad/s):"))

        self.spin_speed = QDoubleSpinBox()
        self.spin_speed.setRange(0.2, self.max_speed)
        self.spin_speed.setSingleStep(0.1)
        self.spin_speed.setDecimals(1)
        self.spin_speed.setValue(self.default_speed)
        self.spin_speed.setSuffix(" rad/s")
        self.spin_speed.setMinimumWidth(90)
        self.spin_speed.valueChanged.connect(self._on_spin_changed)
        row1.addWidget(self.spin_speed)

        layout.addLayout(row1)

        # Slider (0.2 to 10.0 -> scaled * 10 for integer slider: 2 to 100)
        self.slider = QSlider(Qt.Horizontal)
        self.slider.setRange(2, int(self.max_speed * 10))
        self.slider.setValue(int(self.default_speed * 10))
        self.slider.setTickPosition(QSlider.TicksBelow)
        self.slider.setTickInterval(10)
        self.slider.valueChanged.connect(self._on_slider_changed)
        layout.addWidget(self.slider)

        # Row 2: Turn Rate Scaling
        row2 = QHBoxLayout()
        row2.addWidget(QLabel("Turn Ratio:"))

        self.turn_slider = QSlider(Qt.Horizontal)
        self.turn_slider.setRange(30, 100)  # 30% to 100% of linear speed
        self.turn_slider.setValue(70)        # default 70%
        self.turn_slider.valueChanged.connect(self._on_turn_slider_changed)
        row2.addWidget(self.turn_slider)

        self.lbl_turn = QLabel("70%")
        self.lbl_turn.setMinimumWidth(35)
        row2.addWidget(self.lbl_turn)

        layout.addLayout(row2)

        # Row 3: Presets
        presets_row = QHBoxLayout()
        presets = [("Creep (1.0)", 1.0), ("Normal (3.0)", 3.0), ("Fast (5.0)", 5.0), ("Max (10.0)", 10.0)]
        for label, val in presets:
            btn = QPushButton(label)
            btn.setStyleSheet("font-size: 11px; padding: 4px;")
            btn.clicked.connect(lambda _, v=val: self.set_speed(v))
            presets_row.addWidget(btn)

        layout.addLayout(presets_row)

    def _on_slider_changed(self, int_val: int) -> None:
        float_val = int_val / 10.0
        self.spin_speed.blockSignals(True)
        self.spin_speed.setValue(float_val)
        self.spin_speed.blockSignals(False)
        self.speed_changed.emit(float_val)

    def _on_spin_changed(self, float_val: float) -> None:
        self.slider.blockSignals(True)
        self.slider.setValue(int(float_val * 10))
        self.slider.blockSignals(False)
        self.speed_changed.emit(float_val)

    def _on_turn_slider_changed(self, val: int) -> None:
        self.lbl_turn.setText(f"{val}%")

    def set_speed(self, speed_val: float) -> None:
        clamped = max(0.2, min(self.max_speed, speed_val))
        self.spin_speed.setValue(clamped)

    def get_linear_speed(self) -> float:
        return self.spin_speed.value()

    def get_turn_speed(self) -> float:
        ratio = self.turn_slider.value() / 100.0
        return self.spin_speed.value() * ratio
