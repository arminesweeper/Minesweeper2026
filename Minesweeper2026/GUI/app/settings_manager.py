"""
Settings Manager for Minesweeper Control Station.
Handles loading, validating, and persisting JSON settings with fallbacks.
"""

import json
import logging
from pathlib import Path
from typing import Any, Dict

logger = logging.getLogger("ControlStation.Settings")

DEFAULT_SETTINGS: Dict[str, Any] = {
    "serial": {
        "port": "",
        "baud_rate": 115200,
        "timeout_s": 0.05,
        "write_timeout_s": 0.5,
        "auto_reconnect": False,
        "reconnect_interval_s": 2.0,
    },
    "limits": {
        "max_velocity_rad_s": 10.0,
        "default_velocity_rad_s": 3.0,
        "min_velocity_rad_s": 0.2,
        "test_mode_velocity_rad_s": 1.5,
        "command_rate_hz": 10.0,
        "stale_timeout_s": 1.5,
    },
    "servo": {
        "default_angle_deg": 90,
        "min_angle_deg": 0,
        "max_angle_deg": 180,
    },
    "ui": {
        "theme": "dark",
        "window_width": 1280,
        "window_height": 820,
        "max_console_lines": 1000,
        "auto_scroll_console": True,
    },
    "keybindings": {
        "forward": "W",
        "backward": "S",
        "turn_left": "A",
        "turn_right": "D",
        "stop": "Space",
        "estop": "Esc",
    },
}


class SettingsManager:
    """Manages application settings persistence and schema validation."""

    def __init__(self, config_path: Path | None = None):
        if config_path is None:
            config_path = Path(__file__).resolve().parent.parent / "config" / "default_settings.json"
        self.config_path = config_path
        self._data: Dict[str, Any] = self._load()

    def _load(self) -> Dict[str, Any]:
        """Load settings from JSON, returning defaults on any error."""
        if not self.config_path.exists():
            logger.info("Config file not found at %s. Using default settings.", self.config_path)
            return dict(DEFAULT_SETTINGS)

        try:
            with open(self.config_path, "r", encoding="utf-8") as f:
                loaded = json.load(f)
            # Merge with defaults to guarantee all expected keys exist
            merged = self._deep_merge(DEFAULT_SETTINGS, loaded)
            return merged
        except Exception as ex:
            logger.warning("Error loading config from %s: %s. Using defaults.", self.config_path, ex)
            return dict(DEFAULT_SETTINGS)

    def _deep_merge(self, base: Dict[str, Any], override: Dict[str, Any]) -> Dict[str, Any]:
        """Recursively merge base dictionary with overrides."""
        res = dict(base)
        for k, v in override.items():
            if k in res and isinstance(res[k], dict) and isinstance(v, dict):
                res[k] = self._deep_merge(res[k], v)
            else:
                res[k] = v
        return res

    def get(self, section: str, key: str, default: Any = None) -> Any:
        """Retrieve a specific setting."""
        return self._data.get(section, {}).get(key, default)

    def set(self, section: str, key: str, value: Any) -> None:
        """Update a setting in memory."""
        if section not in self._data:
            self._data[section] = {}
        self._data[section][key] = value

    def save(self) -> bool:
        """Persist current settings to JSON file."""
        try:
            self.config_path.parent.mkdir(parents=True, exist_ok=True)
            with open(self.config_path, "w", encoding="utf-8") as f:
                json.dump(self._data, f, indent=2)
            logger.debug("Settings successfully saved to %s", self.config_path)
            return True
        except Exception as ex:
            logger.error("Failed to save settings to %s: %s", self.config_path, ex)
            return False

    @property
    def data(self) -> Dict[str, Any]:
        return self._data
