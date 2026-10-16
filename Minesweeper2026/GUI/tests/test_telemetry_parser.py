"""
Unit tests for telemetry parsing from Arduino Mega serial stream.
Tests high-rate packets, low-rate packets, asynchronous events, fragments, and malformed inputs.
"""

import pytest
from app.protocol.telemetry_parser import (
    BatteryPacket,
    DiagnosticsPacket,
    ErrorPacket,
    HighRateTelemetryPacket,
    IMUPacket,
    LiftStatePacket,
    MetalDetectPacket,
    OdometryPacket,
    ProximityPacket,
    StatusPacket,
    TelemetryParser,
)


def test_parse_high_rate_drive_telemetry():
    parser = TelemetryParser()
    packets = parser.feed("rp02.50,lp02.50,go,\n")
    assert len(packets) == 1
    pkt = packets[0]
    assert isinstance(pkt, HighRateTelemetryPacket)
    assert pytest.approx(pkt.measured_right, 0.01) == 2.50
    assert pytest.approx(pkt.measured_left, 0.01) == 2.50
    assert pkt.gripper_state == "o"


def test_parse_high_rate_drive_negative_velocities():
    parser = TelemetryParser()
    packets = parser.feed("rn01.25,ln03.75,gc,\n")
    assert len(packets) == 1
    pkt = packets[0]
    assert pytest.approx(pkt.measured_right, 0.01) == -1.25
    assert pytest.approx(pkt.measured_left, 0.01) == -3.75
    assert pkt.gripper_state == "c"


def test_parse_odometry():
    parser = TelemetryParser()
    packets = parser.feed("O:1.2345,0.6789,3.1415\n")
    assert len(packets) == 1
    pkt = packets[0]
    assert isinstance(pkt, OdometryPacket)
    assert pytest.approx(pkt.x, 0.0001) == 1.2345
    assert pytest.approx(pkt.y, 0.0001) == 0.6789
    assert pytest.approx(pkt.theta, 0.0001) == 3.1415


def test_parse_battery():
    parser = TelemetryParser()
    packets = parser.feed("B:11.85\n")
    assert len(packets) == 1
    pkt = packets[0]
    assert isinstance(pkt, BatteryPacket)
    assert pytest.approx(pkt.voltage, 0.01) == 11.85


def test_parse_imu():
    parser = TelemetryParser()
    packets = parser.feed("I:45.0,2.1,-1.5\n")
    assert len(packets) == 1
    pkt = packets[0]
    assert isinstance(pkt, IMUPacket)
    assert pytest.approx(pkt.yaw, 0.1) == 45.0
    assert pytest.approx(pkt.pitch, 0.1) == 2.1
    assert pytest.approx(pkt.roll, 0.1) == -1.5


def test_parse_proximity():
    parser = TelemetryParser()
    packets = parser.feed("P:120,450,620,110,85\n")
    assert len(packets) == 1
    pkt = packets[0]
    assert isinstance(pkt, ProximityPacket)
    assert pkt.values == [120, 450, 620, 110, 85]


def test_parse_metal_detect():
    parser = TelemetryParser()
    p1 = parser.feed("M:1\n")
    assert len(p1) == 1 and isinstance(p1[0], MetalDetectPacket) and p1[0].detected is True

    p0 = parser.feed("M:0\n")
    assert len(p0) == 1 and isinstance(p0[0], MetalDetectPacket) and p0[0].detected is False


def test_parse_lift_state():
    parser = TelemetryParser()
    packets = parser.feed("L:RAISING,1F\n")
    assert len(packets) == 1
    pkt = packets[0]
    assert isinstance(pkt, LiftStatePacket)
    assert pkt.state_str == "RAISING"
    assert pkt.magnet_mask == 0x1F


def test_parse_diagnostics():
    parser = TelemetryParser()
    packets = parser.feed("D:50,0,0\n")
    assert len(packets) == 1
    pkt = packets[0]
    assert isinstance(pkt, DiagnosticsPacket)
    assert pkt.loop_hz == 50
    assert pkt.cpu_percent == 0
    assert pkt.fault_code == 0


def test_parse_status_and_error():
    parser = TelemetryParser()
    p_stat = parser.feed("S:PID Enabled\n")
    assert len(p_stat) == 1 and isinstance(p_stat[0], StatusPacket)
    assert p_stat[0].message == "PID Enabled"

    p_err = parser.feed("E:2:Watchdog Timeout\n")
    assert len(p_err) == 1 and isinstance(p_err[0], ErrorPacket)
    assert p_err[0].code == 2
    assert p_err[0].message == "Watchdog Timeout"


def test_fragmented_stream_feeding():
    parser = TelemetryParser()
    # Feed message in 3 chunks
    r1 = parser.feed("rp05.00,")
    assert len(r1) == 0  # incomplete

    r2 = parser.feed("lp05.00,")
    assert len(r2) == 0  # incomplete

    r3 = parser.feed("gm,\n")
    assert len(r3) == 1  # complete now!
    assert isinstance(r3[0], HighRateTelemetryPacket)
    assert pytest.approx(r3[0].measured_right, 0.01) == 5.00


def test_multiple_messages_in_one_read():
    parser = TelemetryParser()
    combined = "B:12.10\nM:1\nO:0.1,0.2,0.3\n"
    packets = parser.feed(combined)
    assert len(packets) == 3
    assert isinstance(packets[0], BatteryPacket)
    assert isinstance(packets[1], MetalDetectPacket)
    assert isinstance(packets[2], OdometryPacket)


def test_malformed_and_garbage_handling():
    parser = TelemetryParser()
    # Garbage should not raise any unhandled exceptions
    packets = parser.feed("???GARBAGE\r\nB:NotANumber\nO:1,2\n\n\n")
    assert isinstance(packets, list)
