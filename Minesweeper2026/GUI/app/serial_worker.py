"""
Serial Worker implementation for PySide6 desktop GUI.
Runs serial I/O in a background QThread to ensure zero GUI blocking.
Supports real hardware COM ports and software simulation mode.
"""

import logging
import queue
import time
from typing import List, Optional
import serial
import serial.tools.list_ports
from PySide6.QtCore import QObject, QThread, Signal, Slot

from .protocol.telemetry_parser import TelemetryParser

logger = logging.getLogger("ControlStation.SerialWorker")


class SerialWorker(QObject):
    """Worker object living on a background QThread for serial I/O."""

    # Signals
    connection_changed = Signal(str, str)  # status_name, info_details
    data_received = Signal(str)            # raw string received
    data_sent = Signal(str)                # raw string sent
    telemetry_packet = Signal(object)      # parsed packet
    error_occurred = Signal(str)           # error text

    def __init__(self, simulation_mode: bool = False):
        super().__init__()
        self.simulation_mode = simulation_mode
        self._running = False
        self._port: Optional[serial.Serial] = None
        self._send_queue: queue.Queue[bytes] = queue.Queue(maxsize=100)
        self._parser = TelemetryParser()

        # Connection parameters
        self._target_port: str = ""
        self._target_baud: int = 115200
        self._reconnect_enabled: bool = False
        self._reconnect_interval_s: float = 2.0
        self._last_reconnect_attempt: float = 0.0

        # Simulation state
        self._sim_last_telemetry: float = 0.0
        self._sim_battery_v: float = 12.4
        self._sim_meas_r: float = 0.0
        self._sim_meas_l: float = 0.0
        self._sim_target_r: float = 0.0
        self._sim_target_l: float = 0.0

    @Slot(str, int)
    def connect_port(self, port_name: str, baud_rate: int = 115200) -> None:
        """Initiate connection to serial port."""
        self._target_port = port_name
        self._target_baud = baud_rate

        if self.simulation_mode:
            logger.info("Connecting in SIMULATION mode.")
            self.connection_changed.emit("Connected", f"Simulated Port ({port_name})")
            return

        if self._port and self._port.is_open:
            self._close_port()

        try:
            logger.info("Opening serial port %s @ %d bps", port_name, baud_rate)
            self._port = serial.Serial(
                port=port_name,
                baudrate=baud_rate,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                timeout=0.05,
                write_timeout=0.2,
            )
            # Reset parser buffer
            self._parser.reset()
            self.connection_changed.emit("Connected", f"{port_name} @ {baud_rate}")
            logger.info("Serial port %s opened successfully.", port_name)
        except serial.SerialException as ex:
            logger.error("Failed to open port %s: %s", port_name, ex)
            self._close_port()
            self.connection_changed.emit("Connection Error", str(ex))
            self.error_occurred.emit(f"Port {port_name} error: {ex}")
        except Exception as ex:
            logger.exception("Unexpected error opening port %s: %s", port_name, ex)
            self._close_port()
            self.connection_changed.emit("Connection Error", str(ex))
            self.error_occurred.emit(f"Failed to open {port_name}: {ex}")

    @Slot()
    def disconnect_port(self) -> None:
        """Disconnect active serial port."""
        self._reconnect_enabled = False
        if self.simulation_mode:
            self.connection_changed.emit("Disconnected", "Simulation stopped")
            return

        self._close_port()
        self.connection_changed.emit("Disconnected", "User requested disconnect")

    def _close_port(self) -> None:
        if self._port:
            try:
                if self._port.is_open:
                    self._port.close()
            except Exception as ex:
                logger.warning("Error closing serial port: %s", ex)
            finally:
                self._port = None

    @Slot(bytes)
    def send_command(self, cmd_bytes: bytes) -> None:
        """Queue command bytes for non-blocking background transmission."""
        try:
            self._send_queue.put_nowait(cmd_bytes)
        except queue.Full:
            logger.warning("Send queue is full; dropping oldest non-essential command.")
            try:
                # Discard an item to prevent queue lockup
                self._send_queue.get_nowait()
                self._send_queue.put_nowait(cmd_bytes)
            except (queue.Empty, queue.Full):
                pass

    @Slot()
    def run_loop(self) -> None:
        """Main non-blocking execution loop in the worker thread."""
        self._running = True
        logger.info("SerialWorker loop started.")

        while self._running:
            if self.simulation_mode:
                self._step_simulation()
                time.sleep(0.01)
                continue

            # Process real hardware serial
            if self._port and self._port.is_open:
                # 1. Transmit queued commands
                while not self._send_queue.empty():
                    try:
                        cmd = self._send_queue.get_nowait()
                        self._port.write(cmd)
                        self._port.flush()
                        cmd_str = cmd.decode("ascii", errors="replace").strip("\r\n")
                        self.data_sent.emit(cmd_str)
                    except queue.Empty:
                        break
                    except (serial.SerialException, OSError) as ex:
                        logger.error("Serial write error: %s", ex)
                        self._handle_hardware_disconnection(str(ex))
                        break

                # 2. Read incoming serial stream
                if self._port and self._port.is_open:
                    try:
                        if self._port.in_waiting > 0:
                            raw_bytes = self._port.read(self._port.in_waiting)
                            raw_str = raw_bytes.decode("ascii", errors="replace")
                            self.data_received.emit(raw_str)
                            # Parse packets
                            packets = self._parser.feed(raw_str)
                            for pkt in packets:
                                self.telemetry_packet.emit(pkt)
                    except (serial.SerialException, OSError) as ex:
                        logger.error("Serial read error: %s", ex)
                        self._handle_hardware_disconnection(str(ex))

            else:
                # Handle auto-reconnect if enabled
                if self._reconnect_enabled and self._target_port:
                    now = time.time()
                    if now - self._last_reconnect_attempt > self._reconnect_interval_s:
                        self._last_reconnect_attempt = now
                        self.connection_changed.emit("Reconnecting", f"Attempting {self._target_port}")
                        self.connect_port(self._target_port, self._target_baud)

            # Avoid pegging CPU when idle
            time.sleep(0.005)

        # Thread shutdown
        self._close_port()
        logger.info("SerialWorker loop terminated cleanly.")

    def _handle_hardware_disconnection(self, error_msg: str) -> None:
        """Handle unexpected hardware disconnect (e.g. cable pulled)."""
        self._close_port()
        self.connection_changed.emit("Connection Error", f"Device disconnected: {error_msg}")
        self.error_occurred.emit(f"Device disconnected: {error_msg}")

    def _step_simulation(self) -> None:
        """Simulate Arduino Mega telemetry and command execution."""
        # Process send queue
        while not self._send_queue.empty():
            try:
                cmd = self._send_queue.get_nowait()
                cmd_str = cmd.decode("ascii", errors="replace").strip("\r\n")
                self.data_sent.emit(cmd_str)

                # Simulate command effects
                if cmd_str == "CESTOP":
                    self._sim_target_r = 0.0
                    self._sim_target_l = 0.0
                    self.data_received.emit("S:E-Stop Triggered\n")
                    self.telemetry_packet.emit(self._parser.parse_line("S:E-Stop Triggered"))
                elif cmd_str == "CCLEAR":
                    self.data_received.emit("S:Faults Cleared\n")
                    self.telemetry_packet.emit(self._parser.parse_line("S:Faults Cleared"))
                elif cmd_str.startswith("rp") or cmd_str.startswith("rn"):
                    # Extract velocity targets
                    lines = self._parser.feed(cmd_str + "\n")
                    for pkt in lines:
                        # Drive simulated wheel velocities towards target
                        self._sim_target_r = getattr(pkt, "measured_right", 0.0)
                        self._sim_target_l = getattr(pkt, "measured_left", 0.0)
            except queue.Empty:
                break

        # Simulate physics: approach target velocities
        self._sim_meas_r += (self._sim_target_r - self._sim_meas_r) * 0.2
        self._sim_meas_l += (self._sim_target_l - self._sim_meas_l) * 0.2

        now = time.time()
        # High rate telemetry (10 Hz = 100 ms)
        if now - self._sim_last_telemetry >= 0.10:
            self._sim_last_telemetry = now
            # Format high-rate packet
            r_sgn = "p" if self._sim_meas_r >= 0 else "n"
            l_sgn = "p" if self._sim_meas_l >= 0 else "n"
            hr_line = f"r{r_sgn}{abs(self._sim_meas_r):05.2f},l{l_sgn}{abs(self._sim_meas_l):05.2f},gc,\n"
            self.data_received.emit(hr_line)
            for pkt in self._parser.feed(hr_line):
                self.telemetry_packet.emit(pkt)

            # Low rate telemetry (~2 Hz)
            if int(now * 10) % 5 == 0:
                batt_line = f"B:{self._sim_battery_v:0.2f}\n"
                self.data_received.emit(batt_line)
                for pkt in self._parser.feed(batt_line):
                    self.telemetry_packet.emit(pkt)

                prox_line = "P:120,135,110,128,115\n"
                self.data_received.emit(prox_line)
                for pkt in self._parser.feed(prox_line):
                    self.telemetry_packet.emit(pkt)

    def stop(self) -> None:
        """Signal worker thread to exit."""
        self._running = False

    @staticmethod
    def list_available_ports() -> List[str]:
        """Enumerate system COM ports."""
        ports = serial.tools.list_ports.comports()
        return [p.device for p in sorted(ports, key=lambda x: x.device)]
