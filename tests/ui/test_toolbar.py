"""TC-TB: Toolbar tests for fluxland compositor.

Tests toolbar visibility toggle, above/below stacking, and keyboard
focus navigation commands.

Note: FocusToolbar/FocusNextElement/FocusPrevElement/FocusActivate are
keyboard-only actions not supported via IPC. Toolbar visibility toggle
changes runtime state but get_config() reads the config file value, so
we verify commands succeed rather than checking config fields.
"""

import time

import pytest

from lib.ipc_client import CommandError


SETTLE = 0.2  # seconds to wait after commands for state to settle


# ---------------------------------------------------------------------------
# TC-TB-01..04: Toolbar visibility and stacking
# ---------------------------------------------------------------------------

class TestToolbarVisibility:
    """Test toolbar show/hide behavior."""

    def test_toolbar_visible_by_default(self, ipc):
        """Toolbar should be visible in the default configuration."""
        config = ipc.get_config()
        assert config["toolbar_visible"] is True

    def test_toggle_toolbar_visible_succeeds(self, ipc):
        """ToggleToolbarVisible command should succeed."""
        try:
            result = ipc.command("ToggleToolbarVisible")
            assert result["success"] is True
        finally:
            # Restore original state
            ipc.command("ToggleToolbarVisible")
            time.sleep(SETTLE)

    def test_toggle_toolbar_visible_twice_is_idempotent(self, ipc, windows):
        """Toggling toolbar visibility twice should not affect window geometry."""
        win = windows.open("TbToggle")
        time.sleep(SETTLE)

        orig = ipc.get_window_by_title("TbToggle")
        orig_x, orig_y = orig["x"], orig["y"]
        orig_w, orig_h = orig["width"], orig["height"]

        try:
            ipc.command("ToggleToolbarVisible")
            time.sleep(SETTLE)
            ipc.command("ToggleToolbarVisible")
            time.sleep(SETTLE)
        finally:
            # Ensure toolbar is restored even if assertion fails
            config = ipc.get_config()
            if not config["toolbar_visible"]:
                ipc.command("ToggleToolbarVisible")
                time.sleep(SETTLE)

        updated = ipc.get_window_by_title("TbToggle")
        assert updated is not None
        # Geometry should be unchanged after double toggle
        assert abs(updated["x"] - orig_x) <= 5
        assert abs(updated["y"] - orig_y) <= 5
        assert abs(updated["width"] - orig_w) <= 5
        assert abs(updated["height"] - orig_h) <= 5

    def test_toggle_toolbar_above_succeeds(self, ipc):
        """ToggleToolbarAbove should execute without error."""
        try:
            result = ipc.command("ToggleToolbarAbove")
            time.sleep(SETTLE)
            assert result["success"] is True
        finally:
            # Toggle back to restore original stacking
            ipc.command("ToggleToolbarAbove")
            time.sleep(SETTLE)


# ---------------------------------------------------------------------------
# TC-TB-05..08: Toolbar keyboard focus navigation
# ---------------------------------------------------------------------------

class TestToolbarFocusNavigation:
    """Test toolbar keyboard focus commands.

    Note: These actions are keyboard-only and return 'action not supported
    via IPC'. Tests verify this is the expected behavior.
    """

    def test_focus_toolbar_not_supported_via_ipc(self, ipc):
        """FocusToolbar is a keyboard-only action, not available via IPC."""
        with pytest.raises(CommandError, match="not supported"):
            ipc.command("FocusToolbar")

    def test_focus_next_element_not_supported_via_ipc(self, ipc):
        """FocusNextElement is a keyboard-only action, not available via IPC."""
        with pytest.raises(CommandError, match="not supported"):
            ipc.command("FocusNextElement")

    def test_focus_prev_element_not_supported_via_ipc(self, ipc):
        """FocusPrevElement is a keyboard-only action, not available via IPC."""
        with pytest.raises(CommandError, match="not supported"):
            ipc.command("FocusPrevElement")

    def test_focus_activate_not_supported_via_ipc(self, ipc):
        """FocusActivate is a keyboard-only action, not available via IPC."""
        with pytest.raises(CommandError, match="not supported"):
            ipc.command("FocusActivate")


# ---------------------------------------------------------------------------
# TC-TB-09: Window placement with toolbar
# ---------------------------------------------------------------------------

class TestToolbarWindowPlacement:
    """Test that toolbar affects window placement."""

    def test_maximized_window_smaller_than_output(self, ipc, windows):
        """Maximized window should be smaller than output (toolbar takes space)."""
        windows.open("TbPlace")
        time.sleep(SETTLE)

        output = ipc.get_outputs()[0]

        ipc.command("Maximize")
        time.sleep(SETTLE)

        win = ipc.get_window_by_title("TbPlace")
        assert win is not None
        assert win["maximized"] is True
        # With toolbar + decorations, maximized window should be smaller than output
        assert win["height"] < output["height"], (
            f"Maximized height ({win['height']}) should be less than "
            f"output height ({output['height']})"
        )
        assert win["width"] <= output["width"]
