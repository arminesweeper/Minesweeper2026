"""
Telemetry Panel: Displays real-time sensory and diagnostic data received from Arduino Mega.
Includes battery status, wheel speeds, IMU, proximity sensors, metal detection, and odometry.
"""

from typing import List
from PySide6.QtCore import Qt
from PySide6.QtWidgets import (
    QFrame,
    QGridLayout,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QProgressBar,
    QVBoxLayout,
    QWidget,
)
from app.robot_state import RobotState


class TelemetryPanel(QGroupBox):
    """Real-time telemetry dashboard."""

    def __init__(self, parent: QWidget | None = None):
        super().__init__("Telemetry & Robot State", parent)
        self._init_ui()

    def _init_ui(self) -> None:
        layout = QVBoxLayout(self)
        layout.setSpacing(8)

        # 1. Metal Detector Alert Banner (High Visibility for Competition)
        self.badge_metal = QLabel("NO MINE DETECTED")
        self.badge_metal.setAlignment(Qt.AlignCenter)
        self.badge_metal.setStyleSheet(
            "QLabel {"
            "   background-color: #263238;"
            "   color: #b0bec5;"
            "   font-size: 14px;"
            "   font-weight: 900;"
            "   padding: 10px;"
            "   border-radius: 6px;"
            "   border: 2px solid #37474f;"
            "}"
        )
        layout.addWidget(self.badge_metal)

        # 2. Battery & Drive Velocities Grid
        grid = QGridLayout()
        grid.setSpacing(6)

        # Battery Row
        grid.addWidget(QLabel("Battery (3S LiPo):"), 0, 0)
        self.lbl_battery = QLabel("--.- V (0%)")
        self.lbl_battery.setStyleSheet("font-weight: bold;")
        grid.addWidget(self.lbl_battery, 0, 1)

        self.bar_battery = QProgressBar()
        self.bar_battery.setRange(0, 100)
        self.bar_battery.setValue(0)
        self.bar_battery.setTextVisible(False)
        self.bar_battery.setStyleSheet(
            "QProgressBar { height: 14px; border-radius: 3px; background-color: #37474f; }"
            "QProgressBar::chunk { background-color: #4caf50; border-radius: 3px; }"
        )
        grid.addWidget(self.bar_battery, 0, 2)

        # Right Wheel Velocity
        grid.addWidget(QLabel("Right Wheel Speed:"), 1, 0)
        self.lbl_right_vel = QLabel("Meas: 0.00 / Tgt: 0.00 rad/s")
        self.lbl_right_vel.setStyleSheet("font-family: monospace; font-weight: bold;")
        grid.addWidget(self.lbl_right_vel, 1, 1, 1, 2)

        # Left Wheel Velocity
        grid.addWidget(QLabel("Left Wheel Speed:"), 2, 0)
        self.lbl_left_vel = QLabel("Meas: 0.00 / Tgt: 0.00 rad/s")
        self.lbl_left_vel.setStyleSheet("font-family: monospace; font-weight: bold;")
        grid.addWidget(self.lbl_left_vel, 2, 1, 1, 2)

        # Odometry
        grid.addWidget(QLabel("Odometry (X, Y, θ):"), 3, 0)
        self.lbl_odom = QLabel("X: 0.000m | Y: 0.000m | θ: 0.000rad")
        self.lbl_odom.setStyleSheet("font-family: monospace;")
        grid.addWidget(self.lbl_odom, 3, 1, 1, 2)

        # IMU Attitude
        grid.addWidget(QLabel("IMU (Yaw/Pitch/Roll):"), 4, 0)
        self.lbl_imu = QLabel("Y: 0.0° | P: 0.0° | R: 0.0°")
        self.lbl_imu.setStyleSheet("font-family: monospace;")
        grid.addWidget(self.lbl_imu, 4, 1, 1, 2)

        # Lift & Gripper State
        grid.addWidget(QLabel("Lift Mechanism:"), 5, 0)
        self.lbl_lift = QLabel("State: IDLE | Magnets: OFF | Gripper: Closed")
        grid.addWidget(self.lbl_lift, 5, 1, 1, 2)

        # Diagnostics Loop Rate
        grid.addWidget(QLabel("Firmware Diagnostics:"), 6, 0)
        self.lbl_diag = QLabel("Loop: 0 Hz | Fault: None")
        grid.addWidget(self.lbl_diag, 6, 1, 1, 2)

        layout.addLayout(grid)

        # 3. 5x Proximity Sensor Array Bar Meters
        prox_group = QGroupBox("Proximity Sensors (ADC 0-1023 | Warning > 500)")
        prox_layout = QHBoxLayout(prox_group)
        self.prox_bars: List[QProgressBar] = []
        self.prox_labels: List[QLabel] = []

        for i in range(5):
            col = QVBoxLayout()
            col.setAlignment(Qt.AlignCenter)

            lbl = QLabel(f"S{i+1}: 0")
            lbl.setAlignment(Qt.AlignCenter)
            lbl.setStyleSheet("font-size: 10px; font-family: monospace;")

            bar = QProgressBar()
            bar.setOrientation(Qt.Vertical)
            bar.setRange(0, 1023)
            bar.setValue(0)
            bar.setMinimumHeight(60)
            bar.setMaximumWidth(24)
            bar.setTextVisible(False)
            bar.setStyleSheet(
                "QProgressBar { background-color: #263238; border-radius: 2px; }"
                "QProgressBar::chunk { background-color: #00bcd4; }"
            )

            col.addWidget(bar)
            col.addWidget(lbl)
            prox_layout.addLayout(col)

            self.prox_bars.append(bar)
            self.prox_labels.append(lbl)

        layout.addWidget(prox_group)

    def update_telemetry(self, state: RobotState) -> None:
        """Efficient live update of all indicators from RobotState."""
        # Metal detector & Target Detection
        if state.sensors.metal_detected:
            self.badge_metal.setText("⚠️ MINE DETECTED! (METAL ALERT)")
            self.badge_metal.setStyleSheet(
                "QLabel {"
                "   background-color: #b71c1c;"
                "   color: white;"
                "   font-size: 15px;"
                "   font-weight: 900;"
                "   padding: 10px;"
                "   border-radius: 6px;"
                "   border: 3px solid #ff1744;"
                "}"
            )
        elif state.sensors.ultrasonic_distance_cm <= 15:
            self.badge_metal.setText(f"🎯 MINE CAUGHT BENEATH GRIPPER! (US: {state.sensors.ultrasonic_distance_cm} cm)")
            self.badge_metal.setStyleSheet(
                "QLabel {"
                "   background-color: #d84315;"
                "   color: white;"
                "   font-size: 14px;"
                "   font-weight: 900;"
                "   padding: 10px;"
                "   border-radius: 6px;"
                "   border: 3px solid #ffab40;"
                "}"
            )
        elif state.sensors.proximity_triggered:
            self.badge_metal.setText("⚠️ OBSTACLE DETECTED! (PROXIMITY)")
            self.badge_metal.setStyleSheet(
                "QLabel {"
                "   background-color: #e65100;"
                "   color: white;"
                "   font-size: 14px;"
                "   font-weight: bold;"
                "   padding: 10px;"
                "   border-radius: 6px;"
                "   border: 2px solid #ff9100;"
                "}"
            )
        else:
            self.badge_metal.setText("AREA CLEAR — NO TARGET DETECTED")
            self.badge_metal.setStyleSheet(
                "QLabel {"
                "   background-color: #1b5e20;"
                "   color: #a5d6a7;"
                "   font-size: 13px;"
                "   font-weight: bold;"
                "   padding: 10px;"
                "   border-radius: 6px;"
                "   border: 2px solid #2e7d32;"
                "}"
            )

        # Battery
        b = state.battery
        if b.voltage > 5.0:
            self.lbl_battery.setText(f"{b.voltage:0.2f} V ({b.percentage:0.0f}%)")
            self.bar_battery.setValue(int(b.percentage))
            if b.is_critical:
                self.bar_battery.setStyleSheet(
                    "QProgressBar { height: 14px; background-color: #37474f; }"
                    "QProgressBar::chunk { background-color: #d32f2f; }"
                )
                self.lbl_battery.setStyleSheet("color: #ff5252; font-weight: bold;")
            elif b.is_low:
                self.bar_battery.setStyleSheet(
                    "QProgressBar { height: 14px; background-color: #37474f; }"
                    "QProgressBar::chunk { background-color: #f57c00; }"
                )
                self.lbl_battery.setStyleSheet("color: #ffb74d; font-weight: bold;")
            else:
                self.bar_battery.setStyleSheet(
                    "QProgressBar { height: 14px; background-color: #37474f; }"
                    "QProgressBar::chunk { background-color: #4caf50; }"
                )
                self.lbl_battery.setStyleSheet("color: #81c784; font-weight: bold;")
        else:
            self.lbl_battery.setText("Not Connected / 0V")
            self.bar_battery.setValue(0)

        # Wheel speeds
        d = state.drive
        self.lbl_right_vel.setText(f"Meas: {d.measured_right:+05.2f} / Tgt: {d.target_right:+05.2f} rad/s")
        self.lbl_left_vel.setText(f"Meas: {d.measured_left:+05.2f} / Tgt: {d.target_left:+05.2f} rad/s")

        # Odometry
        o = state.odometry
        self.lbl_odom.setText(f"X: {o.x:+0.3f}m | Y: {o.y:+0.3f}m | θ: {o.theta:+0.3f}rad")

        # IMU
        imu = state.imu
        self.lbl_imu.setText(f"Y: {imu.yaw:+0.1f}° | P: {imu.pitch:+0.1f}° | R: {imu.roll:+0.1f}°")

        # Lift
        lift = state.lift
        grip_map = {"o": "Top / Open", "c": "Bottom / Closed", "m": "Moving"}
        grip_txt = grip_map.get(lift.gripper_state, lift.gripper_state)
        mag_txt = f"Active (Mask: 0x{lift.magnet_mask:02X})" if lift.magnet_mask else "OFF"
        us_dist = state.sensors.ultrasonic_distance_cm
        us_txt = f"{us_dist} cm" if us_dist < 250 else "> 25 cm (Clear)"
        self.lbl_lift.setText(f"State: {lift.state_str} | Magnets: {mag_txt} | Gripper: {grip_txt} | US: {us_txt}")

        # Diagnostics
        diag = state.diagnostics
        fault_txt = f"CODE {diag.fault_code}" if diag.fault_code else "None"
        self.lbl_diag.setText(f"Loop: {diag.loop_hz} Hz | CPU: {diag.cpu_percent}% | Fault: {fault_txt}")

        # Proximity sensors
        for i, val in enumerate(state.sensors.proximity[:5]):
            if i < len(self.prox_bars):
                self.prox_bars[i].setValue(val)
                self.prox_labels[i].setText(f"S{i+1}: {val}")
                if val > 500:
                    self.prox_bars[i].setStyleSheet(
                        "QProgressBar { background-color: #263238; border-radius: 2px; }"
                        "QProgressBar::chunk { background-color: #e53935; }"
                    )
                    self.prox_labels[i].setStyleSheet("color: #ff5252; font-weight: bold; font-size: 10px;")
                else:
                    self.prox_bars[i].setStyleSheet(
                        "QProgressBar { background-color: #263238; border-radius: 2px; }"
                        "QProgressBar::chunk { background-color: #00bcd4; }"
                    )
                    self.prox_labels[i].setStyleSheet("color: #eceff1; font-size: 10px;")
