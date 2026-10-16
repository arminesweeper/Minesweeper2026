"""
Unit tests for SafetyManager and state transitions.
Verifies arming, emergency stop latching, reset, and staleness detection.
"""

import time
import pytest
from app.robot_state import ConnectionState, RobotState, SafetyState
from app.safety_manager import SafetyManager


def test_safety_initial_state():
    state = RobotState()
    safety = SafetyManager(robot_state=state)
    assert not safety.is_armed
    assert not safety.is_estop_latched
    assert not safety.can_move()
    assert state.safety_state == SafetyState.DISARMED


def test_arming_requires_connection():
    state = RobotState()
    safety = SafetyManager(robot_state=state)

    # Disconnected -> Cannot arm
    state.connection_state = ConnectionState.DISCONNECTED
    assert safety.arm() is False
    assert not safety.is_armed

    # Connected -> Can arm
    state.connection_state = ConnectionState.CONNECTED
    assert safety.arm() is True
    assert safety.is_armed
    assert safety.can_move()


def test_estop_latching_and_reset():
    state = RobotState()
    state.connection_state = ConnectionState.CONNECTED
    safety = SafetyManager(robot_state=state)

    # Arm first
    assert safety.arm() is True

    # Trigger E-Stop
    safety.trigger_estop("Test E-Stop")
    assert safety.is_estop_latched is True
    assert safety.is_armed is False
    assert safety.can_move() is False
    assert state.safety_state == SafetyState.ESTOP

    # Cannot re-arm while E-Stop is latched!
    assert safety.arm() is False

    # Reset latch
    safety.reset_estop_latch()
    assert safety.is_estop_latched is False
    # Still DISARMED until explicit arm
    assert safety.is_armed is False
    assert state.safety_state == SafetyState.DISARMED

    # Now deliberate arm succeeds
    assert safety.arm() is True
    assert safety.is_armed is True


def test_staleness_detection():
    state = RobotState()
    state.connection_state = ConnectionState.CONNECTED
    # Short timeout for testing
    safety = SafetyManager(robot_state=state, stale_timeout_s=0.1)

    safety.arm()
    assert safety.is_armed is True

    # Fake recent RX
    state.last_rx_time = time.time()
    assert safety.check_staleness() is False
    assert safety.is_armed is True

    # Sleep past timeout
    time.sleep(0.15)
    assert safety.check_staleness() is True
    # Disarmed on stale communication!
    assert safety.is_armed is False
