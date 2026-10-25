"""TC-RULES: Window rules tests for fluxland compositor.

Tests the Remember command which saves the focused window's current
properties to the apps file for future window matching.
"""

import os
import time

import pytest


SETTLE = 0.2  # seconds to wait after commands for state to settle


@pytest.fixture(autouse=True)
def scratch_apps(compositor):
    """Remember writes the apps file, which applies to the next window
    that opens (Fluxbox re-reads it): keep it to this test. In a live
    session that would be the user's own apps file, so don't run."""
    if not compositor.config_dir:
        pytest.skip("Remember would write the live session's apps file")
    path = os.path.join(compositor.config_dir, "apps")
    saved = open(path).read() if os.path.exists(path) else None
    yield path
    if saved is None:
        if os.path.exists(path):
            os.unlink(path)
    else:
        with open(path, "w") as f:
            f.write(saved)


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


class TestRememberApplies:
    """What Remember saved is what the next such window gets."""

    def test_next_window_gets_remembered_state(self, ipc, windows,
                                               scratch_apps):
        windows.open("RememberSticky")
        time.sleep(SETTLE)
        ipc.command("Stick")
        ipc.command("Workspace 2")
        time.sleep(SETTLE)
        ipc.command("Workspace 1")
        time.sleep(SETTLE)
        ipc.command("Focus")
        result = ipc.command("Remember")
        assert result["success"] is True
        text = open(scratch_apps).read()
        assert "(class=foot)" in text and "[Sticky]\t{yes}" in text

        win = windows.open("RememberNext")
        time.sleep(SETTLE)
        win = ipc.get_window_by_title("RememberNext")
        assert win["sticky"] is True
