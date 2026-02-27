"""Tests for window movement and resize IPC commands.

Verifies that MoveRight, MoveLeft, MoveUp, MoveDown, MoveTo,
ResizeTo, ResizeHorizontal, and ResizeVertical commands correctly
alter window geometry.
"""

import time

import pytest


class TestWindowMovement:
    """Verify window movement commands via IPC."""

    def test_move_right(self, ipc, windows):
        """MoveRight 50 increases x position by ~50."""
        win = windows.open("MoveRightTest")
        ipc.command("MoveTo 200 200")
        time.sleep(0.2)
        before = ipc.get_focused_window()

        ipc.command("MoveRight 50")
        time.sleep(0.2)
        after = ipc.get_focused_window()

        assert after["x"] == pytest.approx(before["x"] + 50, abs=5)

    def test_move_left(self, ipc, windows):
        """MoveLeft 50 decreases x position by ~50."""
        win = windows.open("MoveLeftTest")
        ipc.command("MoveTo 200 200")
        time.sleep(0.2)
        before = ipc.get_focused_window()

        ipc.command("MoveLeft 50")
        time.sleep(0.2)
        after = ipc.get_focused_window()

        assert after["x"] == pytest.approx(before["x"] - 50, abs=5)

    def test_move_up(self, ipc, windows):
        """MoveUp 50 decreases y position by ~50."""
        win = windows.open("MoveUpTest")
        ipc.command("MoveTo 200 200")
        time.sleep(0.2)
        before = ipc.get_focused_window()

        ipc.command("MoveUp 50")
        time.sleep(0.2)
        after = ipc.get_focused_window()

        assert after["y"] == pytest.approx(before["y"] - 50, abs=5)

    def test_move_down(self, ipc, windows):
        """MoveDown 50 increases y position by ~50."""
        win = windows.open("MoveDownTest")
        ipc.command("MoveTo 200 200")
        time.sleep(0.2)
        before = ipc.get_focused_window()

        ipc.command("MoveDown 50")
        time.sleep(0.2)
        after = ipc.get_focused_window()

        assert after["y"] == pytest.approx(before["y"] + 50, abs=5)

    def test_move_to(self, ipc, windows):
        """MoveTo 100 200 places window at absolute position (100, 200)."""
        win = windows.open("MoveToTest")
        ipc.command("MoveTo 100 200")
        time.sleep(0.2)
        after = ipc.get_focused_window()

        assert after["x"] == pytest.approx(100, abs=5)
        assert after["y"] == pytest.approx(200, abs=5)


class TestWindowResize:
    """Verify window resize commands via IPC."""

    def test_resize_to(self, ipc, windows):
        """ResizeTo 640 480 sets window to exact size 640x480."""
        win = windows.open("ResizeToTest")
        ipc.command("MoveTo 100 100")
        time.sleep(0.2)

        ipc.command("ResizeTo 640 480")
        time.sleep(0.2)
        after = ipc.get_focused_window()

        assert after["width"] == pytest.approx(640, abs=5)
        # Height tolerance wider due to terminal cell-size quantization
        assert after["height"] == pytest.approx(480, abs=15)

    def test_resize_horizontal(self, ipc, windows):
        """ResizeHorizontal 50 increases window width."""
        win = windows.open("ResizeHTest")
        ipc.command("MoveTo 100 100")
        ipc.command("ResizeTo 400 300")
        time.sleep(0.2)
        before = ipc.get_focused_window()

        ipc.command("ResizeHorizontal 50")
        time.sleep(0.2)
        after = ipc.get_focused_window()

        assert after["width"] > before["width"]
        assert after["width"] == pytest.approx(before["width"] + 50, abs=5)

    def test_resize_vertical(self, ipc, windows):
        """ResizeVertical 50 increases window height."""
        win = windows.open("ResizeVTest")
        ipc.command("MoveTo 100 100")
        ipc.command("ResizeTo 400 300")
        time.sleep(0.2)
        before = ipc.get_focused_window()

        ipc.command("ResizeVertical 50")
        time.sleep(0.2)
        after = ipc.get_focused_window()

        assert after["height"] > before["height"]
        # Height tolerance wider due to terminal cell-size quantization
        assert after["height"] == pytest.approx(before["height"] + 50, abs=15)
