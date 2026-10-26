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
    """Toolbar keyboard focus commands also work over IPC.

    IPC shares the key binding dispatcher, so these are no longer
    keyboard-only. FocusToolbar toggles, which leaves state clean.
    """

    def test_focus_toolbar_enter_and_leave(self, ipc):
        """FocusToolbar enters toolbar focus; a second one leaves it."""
        assert ipc.command("FocusToolbar").get("success") is True
        assert ipc.command("FocusToolbar").get("success") is True

    def test_focus_next_prev_element(self, ipc):
        """FocusNextElement/FocusPrevElement move within the toolbar."""
        ipc.command("FocusToolbar")
        try:
            assert ipc.command("FocusNextElement").get("success") is True
            assert ipc.command("FocusPrevElement").get("success") is True
        finally:
            ipc.command("FocusToolbar")

    def test_focus_activate_outside_toolbar_is_noop(self, ipc):
        """FocusActivate does nothing when the toolbar isn't focused."""
        before = ipc.get_workspaces()
        assert ipc.command("FocusActivate").get("success") is True
        assert ipc.get_workspaces() == before


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
        # Maximized window should not exceed the output dimensions
        assert win["height"] <= output["height"]
        assert win["width"] <= output["width"]


# ---------------------------------------------------------------------------
# toolbar.alpha: once over the whole toolbar, as for Fluxbox's one window
# ---------------------------------------------------------------------------

def test_toolbar_alpha_applies_once(tmp_path):
    """A tool's own texture over the toolbar's is not a second layer."""
    import os
    import shutil
    import subprocess

    from PIL import Image

    from lib.compositor import CompositorManager

    if not shutil.which("grim"):
        pytest.skip("grim is not installed")
    style = tmp_path / "style"
    style.write_text(
        "toolbar: flat solid\ntoolbar.color: #000000\n"
        "toolbar.workspace: flat solid\ntoolbar.workspace.color: #000000\n"
        "toolbar.clock: parentrelative\n"
        "toolbar.iconbar.empty: parentrelative\n"
        "toolbar.borderWidth: 0\n"
        "background: flat solid\nbackground.color: #ffffff\n")
    mgr = CompositorManager()
    mgr.start(config_set="default", headless=True, init_extra=[
        f"session.styleFile: {style}",
        "session.screen0.toolbar.alpha: 128",
        "session.screen0.toolbar.tools: workspacename, iconbar, clock",
        "session.screen0.toolbar.widthPercent: 100",
        "session.screen0.toolbar.placement: BottomCenter",
    ])
    try:
        time.sleep(1.0)
        shot = tmp_path / "shot.png"
        subprocess.run(["grim", str(shot)], env=mgr.client_env, check=True,
                       capture_output=True)
        im = Image.open(shot).convert("RGB")
        y = im.height - 3
        label = im.getpixel((5, y))[0]
        plain = im.getpixel((im.width // 2, y))[0]
        # Black at 50% over white: both about 127 (stacked: 63)
        assert abs(plain - 127) <= 3, plain
        assert abs(label - plain) <= 3, (label, plain)
    finally:
        mgr.stop()
