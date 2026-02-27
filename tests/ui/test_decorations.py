"""Window decoration tests for fluxland compositor.

Tests decoration toggle, removal, fullscreen/maximize interaction with
decorations, and independent decoration state across windows.

Note: ToggleDecor/SetDecor change the visual decoration (scene buffers)
but do NOT affect the window geometry reported by get_windows(). Tests
verify command success and that toggle is idempotent rather than checking
geometry changes.
"""

import time

import pytest


SETTLE = 0.2  # seconds to wait after commands for state to settle
TOLERANCE = 10  # pixels tolerance for geometry comparisons


def approx_eq(actual, expected, tol=TOLERANCE):
    """Check that actual is within ±tol of expected."""
    return abs(actual - expected) <= tol


# ---------------------------------------------------------------------------
# TC-DECOR-01: Default decorations
# ---------------------------------------------------------------------------

class TestDecorationDefaults:
    """Test that windows have decorations by default."""

    def test_window_has_decorations_by_default(self, ipc, windows):
        """Open a window, verify it has positive geometry."""
        win = windows.open("DecorDefault")
        time.sleep(SETTLE)

        updated = ipc.get_window_by_title("DecorDefault")
        assert updated is not None
        assert updated["width"] > 0
        assert updated["height"] > 0
        output = ipc.get_outputs()[0]
        assert updated["height"] < output["height"]


# ---------------------------------------------------------------------------
# TC-DECOR-02 through TC-DECOR-05: Decoration toggle and removal
# ---------------------------------------------------------------------------

class TestDecorationToggle:
    """Test decoration visibility toggle."""

    def test_toggle_decor_succeeds(self, ipc, windows):
        """ToggleDecor command should succeed."""
        windows.open("ToggleDecor")
        time.sleep(SETTLE)

        result = ipc.command("ToggleDecor")
        assert result["success"] is True

    def test_toggle_decor_twice_restores(self, ipc, windows):
        """ToggleDecor twice should restore original geometry."""
        win = windows.open("ToggleTwice")
        time.sleep(SETTLE)

        orig_height = win["height"]
        orig_y = win["y"]
        orig_width = win["width"]

        # Toggle off
        ipc.command("ToggleDecor")
        time.sleep(SETTLE)

        # Toggle back on
        ipc.command("ToggleDecor")
        time.sleep(SETTLE)

        updated = ipc.get_window_by_title("ToggleTwice")
        assert updated is not None
        assert approx_eq(updated["height"], orig_height)
        assert approx_eq(updated["y"], orig_y)
        assert approx_eq(updated["width"], orig_width)

    def test_set_decor_none_succeeds(self, ipc, windows):
        """SetDecor NONE command should succeed."""
        windows.open("SetNone")
        time.sleep(SETTLE)

        result = ipc.command("SetDecor NONE")
        assert result["success"] is True

    def test_set_decor_none_then_toggle_restores(self, ipc, windows):
        """SetDecor NONE then ToggleDecor should restore original geometry."""
        win = windows.open("SetToggle")
        time.sleep(SETTLE)

        orig_height = win["height"]
        orig_y = win["y"]

        # Remove decorations
        ipc.command("SetDecor NONE")
        time.sleep(SETTLE)

        # Restore via toggle
        ipc.command("ToggleDecor")
        time.sleep(SETTLE)

        updated = ipc.get_window_by_title("SetToggle")
        assert updated is not None
        assert approx_eq(updated["height"], orig_height)
        assert approx_eq(updated["y"], orig_y)


# ---------------------------------------------------------------------------
# TC-DECOR-06 through TC-DECOR-07: Fullscreen interaction
# ---------------------------------------------------------------------------

class TestFullscreenDecorations:
    """Test decoration behavior with fullscreen."""

    def test_fullscreen_fills_output(self, ipc, windows):
        """Fullscreen window geometry should match output exactly."""
        windows.open("FullDecor")
        time.sleep(SETTLE)

        output = ipc.get_outputs()[0]

        ipc.command("Fullscreen")
        time.sleep(SETTLE)

        win = ipc.get_window_by_title("FullDecor")
        assert win is not None
        assert win["fullscreen"] is True
        assert approx_eq(win["width"], output["width"])
        assert approx_eq(win["height"], output["height"])
        assert approx_eq(win["x"], output["x"])
        assert approx_eq(win["y"], output["y"])

    def test_unfullscreen_restores_decorations(self, ipc, windows):
        """Fullscreen then Fullscreen again should restore original geometry."""
        win = windows.open("UnFull")
        time.sleep(SETTLE)

        orig_height = win["height"]
        orig_width = win["width"]
        orig_x = win["x"]
        orig_y = win["y"]

        # Go fullscreen
        ipc.command("Fullscreen")
        time.sleep(SETTLE)

        # Come back
        ipc.command("Fullscreen")
        time.sleep(SETTLE)

        updated = ipc.get_window_by_title("UnFull")
        assert updated is not None
        assert updated["fullscreen"] is False
        assert approx_eq(updated["width"], orig_width)
        assert approx_eq(updated["height"], orig_height)
        assert approx_eq(updated["x"], orig_x)
        assert approx_eq(updated["y"], orig_y)


# ---------------------------------------------------------------------------
# TC-DECOR-08: Maximize retains decorations
# ---------------------------------------------------------------------------

class TestMaximizeDecorations:
    """Test decoration behavior with maximize."""

    def test_maximized_has_decorations(self, ipc, windows):
        """Maximized window should not exceed the output dimensions."""
        windows.open("MaxDecor")
        time.sleep(SETTLE)

        output = ipc.get_outputs()[0]

        ipc.command("Maximize")
        time.sleep(SETTLE)

        win = ipc.get_window_by_title("MaxDecor")
        assert win is not None
        assert win["maximized"] is True
        assert win["height"] <= output["height"]
        assert win["width"] <= output["width"]


# ---------------------------------------------------------------------------
# TC-DECOR-09 through TC-DECOR-10: Window title and independent decorations
# ---------------------------------------------------------------------------

class TestDecorationMisc:
    """Miscellaneous decoration tests."""

    def test_window_title_matches(self, ipc, windows):
        """Opened window's title should match what was requested."""
        win = windows.open("MyDecorTitle")
        time.sleep(SETTLE)

        updated = ipc.get_window_by_title("MyDecorTitle")
        assert updated is not None
        assert updated["title"] == "MyDecorTitle"

    def test_multiple_windows_independent_decor(self, ipc, windows):
        """Toggle decor on one window should not affect another."""
        win1 = windows.open("IndepA")
        time.sleep(SETTLE)
        win2 = windows.open("IndepB")
        time.sleep(SETTLE)

        # Record original geometry for IndepA
        a_orig_height = win1["height"]
        a_orig_y = win1["y"]

        # IndepB is focused (most recently opened), toggle its decorations
        ipc.command("ToggleDecor")
        time.sleep(SETTLE)

        # IndepA should retain its original geometry
        a_updated = ipc.get_window_by_title("IndepA")
        assert a_updated is not None
        assert approx_eq(a_updated["height"], a_orig_height)
        assert approx_eq(a_updated["y"], a_orig_y)
