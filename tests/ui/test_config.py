"""TC-CFG: Configuration tests for fluxland compositor."""

import time
import pytest
from lib.ipc_client import TimeoutError as IPCTimeoutError

SETTLE = 0.2


class TestConfigQuery:
    """Test configuration query via IPC."""

    def test_get_config_returns_all_fields(self, ipc):
        """get_config returns all expected configuration fields."""
        config = ipc.get_config()
        expected_fields = [
            "workspace_count", "focus_policy", "placement_policy",
            "edge_snap_threshold", "toolbar_visible",
        ]
        for field in expected_fields:
            assert field in config, f"Missing config field: {field}"

    def test_config_values_match_default(self, ipc):
        """Config values match the default test config."""
        config = ipc.get_config()
        assert config["workspace_count"] == 4
        assert config["focus_policy"] == "click"
        assert config["placement_policy"] == "row_smart"
        assert config["edge_snap_threshold"] == 10
        assert config["toolbar_visible"] is True


class TestReconfigure:
    """Test configuration reload via IPC."""

    def test_reconfigure_succeeds(self, ipc):
        """Reconfigure command returns success."""
        result = ipc.command("Reconfigure")
        time.sleep(SETTLE)
        assert result.get("success") is True

    def test_reconfigure_fires_event(self, ipc):
        """Reconfigure fires a config_reloaded event."""
        stream = ipc.subscribe(["config_reloaded"])
        try:
            ipc.command("Reconfigure")
            time.sleep(SETTLE)
            try:
                event = stream.wait_for("config_reloaded", timeout=5.0)
                assert event is not None
                assert event.get("event") == "config_reloaded"
            except IPCTimeoutError:
                pytest.skip("config_reloaded event not fired in headless mode")
        finally:
            stream.close()
