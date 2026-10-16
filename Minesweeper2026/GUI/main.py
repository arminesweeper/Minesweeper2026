"""
Minesweeper Robot Control Station — Desktop GUI Entry Point
Direct USB Serial Interface to Arduino Mega 2560.
"""

import argparse
import logging
import sys
from pathlib import Path

# Ensure GUI root directory is in sys.path
gui_root = Path(__file__).resolve().parent
if str(gui_root) not in sys.path:
    sys.path.insert(0, str(gui_root))

from PySide6.QtCore import Qt
from PySide6.QtWidgets import QApplication

from app.application_controller import ApplicationController
from app.logging_config import setup_logging
from app.main_window import MainWindow
from app.settings_manager import SettingsManager


def parse_args():
    parser = argparse.ArgumentParser(
        description="Minesweeper Robot Serial Control Station"
    )
    parser.add_argument(
        "--sim", action="store_true", help="Launch in hardware simulation mode"
    )
    parser.add_argument(
        "--port", type=str, default="", help="Initial COM port (e.g. COM3 or /dev/ttyUSB0)"
    )
    parser.add_argument(
        "--baud", type=int, default=115200, help="Initial baud rate (default: 115200)"
    )
    parser.add_argument(
        "--debug", action="store_true", help="Enable verbose debug logging"
    )
    return parser.parse_args()


def main():
    args = parse_args()

    # Configure logging
    log_level = logging.DEBUG if args.debug else logging.INFO
    logger = setup_logging(log_level=log_level)
    logger.info("Starting Minesweeper Control Station...")

    # Enable High DPI scaling
    QApplication.setHighDpiScaleFactorRoundingPolicy(
        Qt.HighDpiScaleFactorRoundingPolicy.PassThrough
    )
    app = QApplication(sys.argv)
    app.setApplicationName("MinesweeperControlStation")
    app.setOrganizationName("AssiutRoboticsTeam")

    # Load configuration
    config_file = gui_root / "config" / "default_settings.json"
    settings = SettingsManager(config_path=config_file)

    # Initialize controller
    controller = ApplicationController(
        settings=settings,
        simulation=args.sim,
    )

    # Auto-connect if port specified
    if args.port:
        controller.connect_serial(args.port, args.baud)

    # Create & display main window
    window = MainWindow(controller=controller, settings=settings)
    window.show()

    logger.info("GUI window displayed. Entering Qt event loop.")
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
