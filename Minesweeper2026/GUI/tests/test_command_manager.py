"""
Unit tests for CommandManager arbitration, prioritization, and rate limiting.
"""

import time
import pytest
from app.command_manager import CommandManager
from app.protocol.robot_protocol import RobotProtocol
from app.robot_state import ConnectionState, RobotState
from app.safety_manager import SafetyManager


def test_command_manager_priority_estop():
    sent_commands = []

    def mock_send(cmd: bytes):
        sent_commands.append(cmd)

    state = RobotState()
    state.connection_state = ConnectionState.CONNECTED
    safety = SafetyManager(state)
    safety.arm()

    proto = RobotProtocol()
    cm = CommandManager(protocol=proto, safety=safety, send_fn=mock_send, rate_hz=10.0)

    # Issue movement
    cm.set_drive_targets(3.0, 3.0, immediate=True)
    assert len(sent_commands) == 1
    assert sent_commands[-1] == b"rp03.00,lp03.00,\n"

    # Now trigger E-Stop
    cm.trigger_estop()
    # Must send CESTOP and stop drive
    assert b"CESTOP\n" in sent_commands
    assert b"rp00.00,lp00.00,\n" in sent_commands
    assert safety.is_estop_latched is True

    # Subsequent movement must be rejected
    res = cm.set_drive_targets(5.0, 5.0, immediate=True)
    assert res is False


def test_command_manager_disarmed_rejection():
    sent_commands = []
    state = RobotState()
    safety = SafetyManager(state)  # DISARMED
    proto = RobotProtocol()
    cm = CommandManager(
        protocol=proto, safety=safety, send_fn=lambda c: sent_commands.append(c)
    )

    # Movement rejected when disarmed
    assert cm.set_drive_targets(2.0, 2.0, immediate=True) is False
    assert len(sent_commands) == 0


def test_command_manager_rate_limiting():
    sent_commands = []
    state = RobotState()
    state.connection_state = ConnectionState.CONNECTED
    safety = SafetyManager(state)
    safety.arm()

    proto = RobotProtocol()
    # 10 Hz rate limit (100 ms period)
    cm = CommandManager(
        protocol=proto, safety=safety, send_fn=lambda c: sent_commands.append(c), rate_hz=10.0
    )

    # First send immediately
    res1 = cm.set_drive_targets(2.0, 2.0, immediate=False)
    assert res1 is True
    assert len(sent_commands) == 1

    # Second send immediately after should be rate limited
    res2 = cm.set_drive_targets(2.5, 2.5, immediate=False)
    assert res2 is False
    assert len(sent_commands) == 1

    # After sleeping 110 ms, heartbeat should dispatch packet
    time.sleep(0.11)
    cm.update_heartbeat()
    assert len(sent_commands) == 2
