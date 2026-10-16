"""
Unit tests for protocol encoding and command formatting.
Verified against Arduino Mega SerialProtocol.cpp and SystemManager.cpp.
"""

import pytest
from app.protocol.command_encoder import CommandEncoder
from app.protocol.robot_protocol import RobotProtocol


def test_command_encoder_drive_positive():
    encoder = CommandEncoder(max_velocity=10.0)
    cmd = encoder.encode_drive(right_vel=5.0, left_vel=5.0)
    assert cmd == b"rp05.00,lp05.00,\n"


def test_command_encoder_drive_negative():
    encoder = CommandEncoder(max_velocity=10.0)
    cmd = encoder.encode_drive(right_vel=-3.25, left_vel=-4.50)
    assert cmd == b"rn03.25,ln04.50,\n"


def test_command_encoder_drive_mixed():
    encoder = CommandEncoder(max_velocity=10.0)
    # Turning right: right reverse, left forward
    cmd = encoder.encode_drive(right_vel=-2.0, left_vel=2.0)
    assert cmd == b"rn02.00,lp02.00,\n"


def test_command_encoder_drive_clamping():
    encoder = CommandEncoder(max_velocity=10.0)
    # Attempting to exceed 10.0 rad/s limit
    cmd = encoder.encode_drive(right_vel=15.5, left_vel=-20.0)
    assert cmd == b"rp10.00,ln10.00,\n"


def test_command_encoder_drive_with_gripper_and_servo():
    encoder = CommandEncoder(max_velocity=10.0)
    cmd = encoder.encode_drive(
        right_vel=1.0, left_vel=1.0, gripper_vel=1.0, servo_angle=90.0
    )
    assert cmd == b"rp01.00,lp01.00,gp01.00,sp90.00,\n"


def test_command_encoder_stop():
    encoder = CommandEncoder(max_velocity=10.0)
    cmd = encoder.encode_stop()
    assert cmd == b"rp00.00,lp00.00,\n"


def test_command_encoder_extended_commands():
    encoder = CommandEncoder()
    assert encoder.encode_estop() == b"CESTOP\n"
    assert encoder.encode_clear_faults() == b"CCLEAR\n"
    assert encoder.encode_reset() == b"CRESET\n"
    assert encoder.encode_pid(True) == b"CPID:ON\n"
    assert encoder.encode_pid(False) == b"CPID:OFF\n"
    assert encoder.encode_lift_cycle() == b"CLIFT:CYCLE\n"
    assert encoder.encode_lift_lower() == b"CLIFT:DN\n"
    assert encoder.encode_lift_stop() == b"CLIFT:STOP\n"
    assert encoder.encode_magnets(True) == b"CMAGNET:ON\n"
    assert encoder.encode_magnets(False) == b"CMAGNET:OFF\n"
    assert encoder.encode_siren(True) == b"CBUZZ:ALERT\n"
    assert encoder.encode_siren(False) == b"CBUZZ:SILENT\n"
    assert encoder.encode_servo(45) == b"CSERVO:45\n"
    assert encoder.encode_servo(250) == b"CSERVO:180\n"  # Clamped to 180


def test_robot_protocol_wrapper():
    proto = RobotProtocol(max_velocity=10.0)
    assert proto.encode_stop() == b"rp00.00,lp00.00,\n"
    assert proto.encode_estop() == b"CESTOP\n"
    assert proto.encode_drive(2.5, 2.5) == b"rp02.50,lp02.50,\n"
