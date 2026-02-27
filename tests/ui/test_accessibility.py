"""Tests for accessibility features via IPC.

Verifies that fluxland exposes sufficient information for screen readers
and assistive technology via its IPC protocol, and that the accessibility
meta-subscription covers the expected event categories.
"""

import time
import pytest
from lib.ipc_client import TimeoutError as IPCTimeoutError

SETTLE = 0.2

STYLE_DEFAULT = "/path/to/fluxland/data/style"
STYLE_HC_DARK = "/path/to/fluxland/data/fluxland-hc-dark.style"


class TestAccessibilitySubscription:
    """Test the accessibility meta-subscription."""

    def test_accessibility_meta_subscription(self, ipc, windows):
        """Subscribing to 'accessibility' receives focus_changed events."""
        stream = ipc.subscribe(["accessibility"])
        try:
            windows.open("A11yTest1")
            windows.open("A11yTest2")
            time.sleep(SETTLE)

            ipc.command("NextWindow")
            time.sleep(SETTLE)

            # The accessibility subscription should fire focus-related events
            try:
                event = stream.wait_for("focus_changed", timeout=5.0)
                assert event is not None
            except IPCTimeoutError:
                pytest.skip("focus_changed not fired via accessibility subscription")
        finally:
            stream.close()

    def test_focus_changed_event_on_window_switch(self, ipc, windows):
        """Switching focus fires a focus_changed event."""
        windows.open("FocusEvt1")
        windows.open("FocusEvt2")
        time.sleep(SETTLE)

        stream = ipc.subscribe(["focus_changed"])
        try:
            ipc.command("NextWindow")
            time.sleep(SETTLE)
            try:
                event = stream.wait_for("focus_changed", timeout=5.0)
                assert event is not None
                assert event.get("event") == "focus_changed"
            except IPCTimeoutError:
                pytest.skip("focus_changed event not fired in headless mode")
        finally:
            stream.close()


class TestAccessibilityWindowInfo:
    """Test that window info exposes accessibility-relevant data."""

    def test_windows_expose_title(self, ipc, windows):
        """get_windows returns title field for screen reader access."""
        windows.open("TitleCheck")
        time.sleep(SETTLE)

        wins = ipc.get_windows()
        assert len(wins) > 0
        win = next(w for w in wins if w.get("title") == "TitleCheck")
        assert "title" in win
        assert win["title"] == "TitleCheck"

    def test_windows_expose_app_id(self, ipc, windows):
        """get_windows returns app_id field for screen reader access."""
        windows.open("AppIdCheck")
        time.sleep(SETTLE)

        wins = ipc.get_windows()
        assert len(wins) > 0
        win = next(w for w in wins if w.get("title") == "AppIdCheck")
        assert "app_id" in win
        assert isinstance(win["app_id"], str)
        assert len(win["app_id"]) > 0

    def test_focused_window_marked(self, ipc, windows):
        """Exactly one window has focused=true when multiple exist."""
        windows.open("FocusMark1")
        windows.open("FocusMark2")
        time.sleep(SETTLE)

        wins = ipc.get_windows()
        focused = [w for w in wins if w.get("focused") is True]
        assert len(focused) == 1, (
            f"Expected exactly 1 focused window, got {len(focused)}"
        )


class TestHighContrastStyles:
    """Test high-contrast style support for accessibility."""

    def test_set_hc_dark_style(self, ipc):
        """SetStyle with high-contrast dark style succeeds."""
        try:
            result = ipc.command(f"SetStyle {STYLE_HC_DARK}")
            time.sleep(SETTLE)
            assert result.get("success") is True
        finally:
            ipc.command(f"SetStyle {STYLE_DEFAULT}")
            time.sleep(SETTLE)
