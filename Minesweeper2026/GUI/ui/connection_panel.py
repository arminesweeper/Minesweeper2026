"""
Connection Panel: Serial port selection, baud rate configuration,
refreshing, connecting, disconnecting, and connection health status badge.
"""

from PySide6.QtCore import Qt, Signal
from PySide6.QtWidgets import (
    QCheckBox,
    QComboBox,
    QFrame,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QPushButton,
    QVBoxLayout,
    QWidget,
)
from app.serial_worker import SerialWorker


class ConnectionPanel(QGroupBox):
    """UI Panel for establishing and monitoring serial connection."""

    connect_requested = Signal(str, int)  # port, baud
    disconnect_requested = Signal()
    simulation_toggled = Signal(bool)

    BAUD_RATES = ["115200", "57600", "38400", "19200", "9600"]

    def __init__(self, parent: QWidget | None = None):
        super().__init__("Serial Connection", parent)
        self._init_ui()
        self.refresh_ports()

    def _init_ui(self) -> None:
        layout = QVBoxLayout(self)
        layout.setSpacing(8)

        # Row 1: Port and Baud selection
        row1 = QHBoxLayout()

        row1.addWidget(QLabel("Port:"))
        self.port_combo = QComboBox()
        self.port_combo.setMinimumWidth(120)
        row1.addWidget(self.port_combo)

        self.btn_refresh = QPushButton("Refresh")
        self.btn_refresh.setToolTip("Rescan for connected USB serial devices")
        self.btn_refresh.clicked.connect(self.refresh_ports)
        row1.addWidget(self.btn_refresh)

        row1.addWidget(QLabel("Baud:"))
        self.baud_combo = QComboBox()
        self.baud_combo.addItems(self.BAUD_RATES)
        self.baud_combo.setCurrentText("115200")
        row1.addWidget(self.baud_combo)

        layout.addLayout(row1)

        # Row 2: Connect / Disconnect and Simulation checkbox
        row2 = QHBoxLayout()

        self.sim_check = QCheckBox("Simulation Mode")
        self.sim_check.setToolTip("Run without physical hardware (simulates robot telemetry)")
        self.sim_check.toggled.connect(self._on_sim_toggled)
        row2.addWidget(self.sim_check)

        row2.addStretch()

        self.btn_connect = QPushButton("Connect")
        self.btn_connect.setStyleSheet(
            "QPushButton { background-color: #2e7d32; color: white; font-weight: bold; padding: 6px 14px; border-radius: 4px; }"
            "QPushButton:hover { background-color: #388e3c; }"
        )
        self.btn_connect.clicked.connect(self._on_connect_clicked)
        row2.addWidget(self.btn_connect)

        self.btn_disconnect = QPushButton("Disconnect")
        self.btn_disconnect.setEnabled(False)
        self.btn_disconnect.setStyleSheet(
            "QPushButton { background-color: #c62828; color: white; font-weight: bold; padding: 6px 14px; border-radius: 4px; }"
            "QPushButton:hover { background-color: #d32f2f; }"
            "QPushButton:disabled { background-color: #555555; color: #888888; }"
        )
        self.btn_disconnect.clicked.connect(self._on_disconnect_clicked)
        row2.addWidget(self.btn_disconnect)

        layout.addLayout(row2)

        # Row 3: Status Badge
        self.status_badge = QLabel("Status: DISCONNECTED")
        self.status_badge.setAlignment(Qt.AlignCenter)
        self.status_badge.setStyleSheet(
            "QLabel { background-color: #37474f; color: #eceff1; padding: 5px; border-radius: 4px; font-weight: bold; font-size: 12px; }"
        )
        layout.addWidget(self.status_badge)

    def refresh_ports(self) -> None:
        """Scan available ports and populate combo box."""
        current = self.port_combo.currentText()
        ports = SerialWorker.list_available_ports()
        self.port_combo.clear()

        if self.sim_check.isChecked():
            self.port_combo.addItem("SIM_VIRTUAL_PORT")
        elif ports:
            self.port_combo.addItems(ports)
            if current in ports:
                self.port_combo.setCurrentText(current)
        else:
            self.port_combo.addItem("No ports found")

    def _on_sim_toggled(self, checked: bool) -> None:
        self.simulation_toggled.emit(checked)
        self.refresh_ports()

    def _on_connect_clicked(self) -> None:
        port = self.port_combo.currentText()
        if not port or port == "No ports found":
            self.set_status("Error", "No valid serial port selected")
            return

        baud = int(self.baud_combo.currentText())
        self.connect_requested.emit(port, baud)

    def _on_disconnect_clicked(self) -> None:
        self.disconnect_requested.emit()

    def set_status(self, status: str, details: str = "") -> None:
        """Update visual badge based on status string."""
        if status == "Connected":
            self.status_badge.setText(f"CONNECTED: {details}")
            self.status_badge.setStyleSheet(
                "QLabel { background-color: #1b5e20; color: #a5d6a7; padding: 5px; border-radius: 4px; font-weight: bold; }"
            )
            self.btn_connect.setEnabled(False)
            self.btn_disconnect.setEnabled(True)
            self.port_combo.setEnabled(False)
            self.baud_combo.setEnabled(False)
            self.sim_check.setEnabled(False)
        elif status == "Reconnecting":
            self.status_badge.setText(f"RECONNECTING: {details}")
            self.status_badge.setStyleSheet(
                "QLabel { background-color: #e65100; color: #ffe0b2; padding: 5px; border-radius: 4px; font-weight: bold; }"
            )
        elif status in ("Connection Error", "Error"):
            self.status_badge.setText(f"ERROR: {details}")
            self.status_badge.setStyleSheet(
                "QLabel { background-color: #b71c1c; color: #ffcdd2; padding: 5px; border-radius: 4px; font-weight: bold; }"
            )
            self.btn_connect.setEnabled(True)
            self.btn_disconnect.setEnabled(False)
            self.port_combo.setEnabled(True)
            self.baud_combo.setEnabled(True)
            self.sim_check.setEnabled(True)
        else:
            self.status_badge.setText("DISCONNECTED")
            self.status_badge.setStyleSheet(
                "QLabel { background-color: #37474f; color: #cfd8dc; padding: 5px; border-radius: 4px; font-weight: bold; }"
            )
            self.btn_connect.setEnabled(True)
            self.btn_disconnect.setEnabled(False)
            self.port_combo.setEnabled(True)
            self.baud_combo.setEnabled(True)
            self.sim_check.setEnabled(True)
