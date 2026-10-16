"""
Serial Console: Real-time diagnostic terminal showing timestamped TX and RX streams.
Supports color-coding, auto-scrolling, pausing, filtering, clearing, and log export.
"""

from datetime import datetime
import logging
from PySide6.QtCore import Qt
from PySide6.QtGui import QColor, QTextCharFormat, QTextCursor
from PySide6.QtWidgets import (
    QCheckBox,
    QFileDialog,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QPlainTextEdit,
    QPushButton,
    QVBoxLayout,
    QWidget,
)

logger = logging.getLogger("ControlStation.Console")


class SerialConsole(QGroupBox):
    """Timestamped diagnostic communication console."""

    def __init__(self, max_lines: int = 1000, parent: QWidget | None = None):
        super().__init__("Serial Communication Terminal", parent)
        self.max_lines = max_lines
        self._is_paused = False
        self._init_ui()

    def _init_ui(self) -> None:
        layout = QVBoxLayout(self)
        layout.setSpacing(6)

        # Console Text Display
        self.text_edit = QPlainTextEdit()
        self.text_edit.setReadOnly(True)
        self.text_edit.setMaximumBlockCount(self.max_lines)
        self.text_edit.setStyleSheet(
            "QPlainTextEdit {"
            "   background-color: #121212;"
            "   color: #e0e0e0;"
            "   font-family: 'Consolas', 'Courier New', monospace;"
            "   font-size: 11px;"
            "   border: 1px solid #263238;"
            "   border-radius: 4px;"
            "}"
        )
        layout.addWidget(self.text_edit)

        # Control Toolbar
        toolbar = QHBoxLayout()

        self.check_autoscroll = QCheckBox("Auto-Scroll")
        self.check_autoscroll.setChecked(True)
        toolbar.addWidget(self.check_autoscroll)

        self.btn_pause = QPushButton("Pause")
        self.btn_pause.setToolTip("Temporarily stop updating terminal text")
        self.btn_pause.clicked.connect(self._toggle_pause)
        toolbar.addWidget(self.btn_pause)

        self.btn_clear = QPushButton("Clear")
        self.btn_clear.clicked.connect(self.text_edit.clear)
        toolbar.addWidget(self.btn_clear)

        toolbar.addStretch()

        self.btn_export = QPushButton("Export Log...")
        self.btn_export.setToolTip("Save visible terminal history to a text file")
        self.btn_export.clicked.connect(self._export_log)
        toolbar.addWidget(self.btn_export)

        layout.addLayout(toolbar)

    def append_message(self, direction: str, text: str) -> None:
        """
        Append timestamped message with color formatting.
        direction: 'TX' or 'RX'
        """
        if self._is_paused:
            return

        clean_text = text.strip("\r\n\0")
        if not clean_text:
            return

        timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        prefix = f"[{timestamp}] "

        cursor = self.text_edit.textCursor()
        cursor.movePosition(QTextCursor.End)

        # Timestamp format
        ts_fmt = QTextCharFormat()
        ts_fmt.setForeground(QColor("#78909c"))
        cursor.insertText(prefix, ts_fmt)

        # Direction and Body format
        body_fmt = QTextCharFormat()
        if direction == "TX":
            body_fmt.setForeground(QColor("#4fc3f7"))  # Light Blue for Sent
            cursor.insertText(f"TX ► {clean_text}\n", body_fmt)
        else:
            if clean_text.startswith("E:"):
                body_fmt.setForeground(QColor("#ef5350"))  # Red for Error
            elif clean_text.startswith("S:"):
                body_fmt.setForeground(QColor("#ffb74d"))  # Amber for Status
            elif clean_text.startswith("M:1"):
                body_fmt.setForeground(QColor("#ff1744"))  # Bright Red for Mine
                body_fmt.setFontWeight(700)
            else:
                body_fmt.setForeground(QColor("#81c784"))  # Green for regular telemetry
            cursor.insertText(f"RX ◄ {clean_text}\n", body_fmt)

        if self.check_autoscroll.isChecked():
            self.text_edit.moveCursor(QTextCursor.End)

    def _toggle_pause(self) -> None:
        self._is_paused = not self._is_paused
        if self._is_paused:
            self.btn_pause.setText("Resume")
            self.btn_pause.setStyleSheet("background-color: #f57c00; color: white;")
        else:
            self.btn_pause.setText("Pause")
            self.btn_pause.setStyleSheet("")

    def _export_log(self) -> None:
        """Export console text to a file chosen by operator."""
        file_path, _ = QFileDialog.getSaveFileName(
            self,
            "Save Terminal Log",
            f"minesweeper_terminal_{datetime.now().strftime('%Y%m%d_%H%M%S')}.txt",
            "Text Files (*.txt);;All Files (*)",
        )
        if file_path:
            try:
                with open(file_path, "w", encoding="utf-8") as f:
                    f.write(self.text_edit.toPlainText())
                logger.info("Terminal log exported to %s", file_path)
            except Exception as ex:
                logger.error("Failed to export terminal log: %s", ex)
