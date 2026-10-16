"""
GUI Integration tests using pytest-qt.
Verifies MainWindow assembly, panel controls, emergency stop interactions, and speed slider.
"""

import pytest
from app.application_controller import ApplicationController
from app.main_window import MainWindow
from app.robot_state import SafetyState
from app.settings_manager import SettingsManager


@pytest.fixture
def test_app(qtbot, tmp_path):
    settings_file = tmp_path / "test_settings.json"
    settings = SettingsManager(config_path=settings_file)
    controller = ApplicationController(settings=settings, simulation=True)
    window = MainWindow(controller=controller, settings=settings)
    qtbot.addWidget(window)
    yield window, controller
    controller.shutdown()


def test_main_window_components_exist(test_app):
    window, controller = test_app
    assert window.panel_conn is not None
    assert window.panel_safety is not None
    assert window.panel_movement is not None
    assert window.panel_speed is not None
    assert window.panel_mech is not None
    assert window.panel_telemetry is not None
    assert window.panel_console is not None


def test_emergency_stop_button_triggers_estop(test_app, qtbot):
    window, controller = test_app

    # Connect in simulation
    window.panel_conn.sim_check.setChecked(True)
    window.panel_conn.btn_connect.click()

    # Arm
    window.panel_safety.btn_arm.click()
    assert controller.safety.is_armed is True

    # Click big E-Stop button
    window.panel_safety.btn_estop.click()

    assert controller.safety.is_estop_latched is True
    assert controller.safety.is_armed is False
    assert controller.robot_state.safety_state == SafetyState.ESTOP


def test_speed_panel_synchronization(test_app):
    window, _ = test_app
    # Set speed via spinbox
    window.panel_speed.spin_speed.setValue(4.5)
    assert window.panel_speed.slider.value() == 45
    assert pytest.approx(window.panel_speed.get_linear_speed(), 0.1) == 4.5
