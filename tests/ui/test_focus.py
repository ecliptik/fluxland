"""Focus behavior tests for fluxland compositor."""

import time

import pytest


class TestFocusBehavior:
    """Test window focus operations."""

    def test_new_window_gets_focus(self, ipc, windows):
        """Opening a window should give it focus."""
        windows.open("FocusNew")
        time.sleep(0.2)

        focused = ipc.get_focused_window()
        assert focused is not None
        assert focused["title"] == "FocusNew"

    def test_next_window_cycles_focus(self, ipc, windows):
        """NextWindow should cycle focus to the next window."""
        windows.open("Win1")
        windows.open("Win2")
        time.sleep(0.2)

        # Win2 should have focus (most recently opened)
        focused = ipc.get_focused_window()
        assert focused is not None
        assert focused["title"] == "Win2"

        # NextWindow should cycle focus to Win1
        ipc.command("NextWindow")
        time.sleep(0.2)

        focused = ipc.get_focused_window()
        assert focused is not None
        assert focused["title"] == "Win1"

    def test_prev_window_cycles_focus(self, ipc, windows):
        """PrevWindow should cycle focus in the reverse direction."""
        windows.open("Win1")
        windows.open("Win2")
        time.sleep(0.2)

        # Win2 should have focus (most recently opened)
        focused = ipc.get_focused_window()
        assert focused is not None
        assert focused["title"] == "Win2"

        # PrevWindow should cycle focus to Win1
        ipc.command("PrevWindow")
        time.sleep(0.2)

        focused = ipc.get_focused_window()
        assert focused is not None
        assert focused["title"] == "Win1"

    def test_second_window_takes_focus(self, ipc, windows):
        """Opening a second window should steal focus from the first."""
        windows.open("Win1")
        time.sleep(0.2)

        # Win1 should have focus
        focused = ipc.get_focused_window()
        assert focused is not None
        assert focused["title"] == "Win1"

        # Open Win2 — it should take focus
        windows.open("Win2")
        time.sleep(0.2)

        focused = ipc.get_focused_window()
        assert focused is not None
        assert focused["title"] == "Win2"

        # Win1 should still exist but no longer be focused
        all_wins = ipc.get_windows()
        titles = [w["title"] for w in all_wins]
        assert "Win1" in titles
        assert len(all_wins) == 2
