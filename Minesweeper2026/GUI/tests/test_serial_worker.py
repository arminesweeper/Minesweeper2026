"""
Unit tests for SerialWorker background transport and simulation mode.
"""

import time
import pytest
from app.protocol.telemetry_parser import HighRateTelemetryPacket
from app.serial_worker import SerialWorker


def test_serial_worker_port_enumeration():
    # Should return a list without crashing
    ports = SerialWorker.list_available_ports()
    assert isinstance(ports, list)


def test_serial_worker_simulation_connection(qtbot):
    worker = SerialWorker(simulation_mode=True)

    status_events = []
    worker.connection_changed.connect(lambda s, d: status_events.append((s, d)))

    worker.connect_port("SIM_PORT", 115200)
    assert len(status_events) > 0
    assert status_events[-1][0] == "Connected"

    worker.disconnect_port()
    assert status_events[-1][0] == "Disconnected"


def test_serial_worker_simulation_step(qtbot):
    worker = SerialWorker(simulation_mode=True)
    telemetry_packets = []
    worker.telemetry_packet.connect(lambda pkt: telemetry_packets.append(pkt))

    worker.connect_port("SIM_PORT", 115200)

    # Queue drive command
    worker.send_command(b"rp02.00,lp02.00,\n")

    # Step simulation multiple times
    for _ in range(15):
        worker._step_simulation()
        time.sleep(0.015)

    # Should have emitted simulated telemetry packets
    assert len(telemetry_packets) > 0
    hr_packets = [p for p in telemetry_packets if isinstance(p, HighRateTelemetryPacket)]
    assert len(hr_packets) > 0
