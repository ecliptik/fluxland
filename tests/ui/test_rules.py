"""TC-RULES: Window rules tests for fluxland compositor.

Tests the Remember command which saves the focused window's current
properties to the apps file for future window matching.
"""

import time

import pytest


SETTLE = 0.2  # seconds to wait after commands for state to settle


class TestRemember:
    """Verify the Remember command succeeds in various window states."""

    def test_remember_succeeds(self, ipc, windows):
        """Remember on a normal window returns success."""
        windows.open("RememberMe")
        time.sleep(SETTLE)

        result = ipc.command("Remember")
        time.sleep(SETTLE)

        assert result["success"] is True

    def test_remember_no_window_succeeds(self, ipc):
        """Remember with no focused window returns success (no-op)."""
        result = ipc.command("Remember")
        time.sleep(SETTLE)

        assert result["success"] is True

    def test_remember_maximized(self, ipc, windows):
        """Remember on a maximized window returns success."""
        windows.open("RememberMax")
        time.sleep(SETTLE)

        ipc.command("Maximize")
        time.sleep(SETTLE)

        result = ipc.command("Remember")
        time.sleep(SETTLE)

        assert result["success"] is True

    def test_remember_with_position(self, ipc, windows):
        """Remember after MoveTo saves position state successfully."""
        windows.open("RememberPos")
        time.sleep(SETTLE)

        ipc.command("MoveTo 100 100")
        time.sleep(SETTLE)

        result = ipc.command("Remember")
        time.sleep(SETTLE)

        assert result["success"] is True

    def test_remember_multiple_windows(self, ipc, windows):
        """Remember succeeds on each of multiple windows."""
        windows.open("RememberA")
        time.sleep(SETTLE)

        result_a = ipc.command("Remember")
        time.sleep(SETTLE)
        assert result_a["success"] is True

        windows.open("RememberB")
        time.sleep(SETTLE)

        result_b = ipc.command("Remember")
        time.sleep(SETTLE)
        assert result_b["success"] is True

    def test_remember_after_resize(self, ipc, windows):
        """Remember after ResizeTo saves resized state successfully."""
        windows.open("RememberResize")
        time.sleep(SETTLE)

        ipc.command("ResizeTo 400 300")
        time.sleep(SETTLE)

        result = ipc.command("Remember")
        time.sleep(SETTLE)

        assert result["success"] is True

    def test_remember_sticky(self, ipc, windows):
        """Remember on a sticky window returns success."""
        windows.open("RememberSticky")
        time.sleep(SETTLE)

        ipc.command("Stick")
        time.sleep(SETTLE)

        result = ipc.command("Remember")
        time.sleep(SETTLE)

        assert result["success"] is True

    def test_remember_after_fullscreen(self, ipc, windows):
        """Remember on a fullscreen window returns success."""
        windows.open("RememberFull")
        time.sleep(SETTLE)

        ipc.command("Fullscreen")
        time.sleep(SETTLE)

        result = ipc.command("Remember")
        time.sleep(SETTLE)

        assert result["success"] is True
