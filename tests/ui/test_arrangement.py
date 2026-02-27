"""TC-ARR: Window arrangement tests for fluxland compositor.

Tests window arrangement operations: grid tiling, vertical/horizontal
stacking, and cascade layout via IPC commands.
"""

import time

import pytest

SETTLE = 0.2
TOLERANCE = 20


class TestWindowArrangement:
    """Test window arrangement and tiling operations."""

    def test_arrange_grid(self, ipc, windows):
        """Arrange 4 windows in a grid and verify distinct positions."""
        windows.open_many(4, prefix="Grid")
        time.sleep(SETTLE)

        ipc.command("ArrangeWindows")
        time.sleep(SETTLE)

        positions = []
        for i in range(4):
            win = ipc.get_window_by_title(f"Grid{i+1}")
            assert win is not None, f"Grid {i+1} not found"
            positions.append((win["x"], win["y"]))
            # Each window should have reasonable size
            assert win["width"] > 50
            assert win["height"] > 50

        # All 4 windows should have distinct positions
        unique_positions = set(positions)
        assert len(unique_positions) == 4, (
            f"Expected 4 distinct positions, got {len(unique_positions)}: {positions}"
        )

    def test_arrange_vertical(self, ipc, windows):
        """ArrangeWindowsVertical creates equal-width vertical columns (side by side)."""
        windows.open_many(3, prefix="Vert")
        time.sleep(SETTLE)

        ipc.command("ArrangeWindowsVertical")
        time.sleep(SETTLE)

        wins = []
        for i in range(3):
            win = ipc.get_window_by_title(f"Vert{i+1}")
            assert win is not None, f"Vert {i+1} not found"
            wins.append(win)

        # Vertical columns share the same y position
        ys = [w["y"] for w in wins]
        for y in ys:
            assert y == pytest.approx(ys[0], abs=TOLERANCE)

        # Columns have different x positions
        xs = sorted(w["x"] for w in wins)
        for i in range(1, len(xs)):
            assert xs[i] > xs[i - 1] + TOLERANCE, (
                f"Windows not in vertical columns: x values {xs}"
            )

    def test_arrange_horizontal(self, ipc, windows):
        """ArrangeWindowsHorizontal creates equal-height horizontal rows (stacked)."""
        windows.open_many(3, prefix="Horiz")
        time.sleep(SETTLE)

        ipc.command("ArrangeWindowsHorizontal")
        time.sleep(SETTLE)

        wins = []
        for i in range(3):
            win = ipc.get_window_by_title(f"Horiz{i+1}")
            assert win is not None, f"Horiz {i+1} not found"
            wins.append(win)

        # Horizontal rows share the same x position
        xs = [w["x"] for w in wins]
        for x in xs:
            assert x == pytest.approx(xs[0], abs=TOLERANCE)

        # Rows have different y positions
        ys = sorted(w["y"] for w in wins)
        for i in range(1, len(ys)):
            assert ys[i] > ys[i - 1] + TOLERANCE, (
                f"Windows not in horizontal rows: y values {ys}"
            )

    def test_cascade(self, ipc, windows):
        """Cascade 3 windows with progressive offset."""
        windows.open_many(3, prefix="Casc")
        time.sleep(SETTLE)

        ipc.command("CascadeWindows")
        time.sleep(SETTLE)

        wins = []
        for i in range(3):
            win = ipc.get_window_by_title(f"Casc{i+1}")
            assert win is not None, f"Casc {i+1} not found"
            wins.append(win)

        # Sort by position to get cascade order
        wins.sort(key=lambda w: (w["x"], w["y"]))

        # Each subsequent window should be offset from the previous
        for i in range(1, len(wins)):
            assert wins[i]["x"] > wins[i - 1]["x"], (
                f"Cascade x not increasing: {wins[i-1]['x']} -> {wins[i]['x']}"
            )
            assert wins[i]["y"] > wins[i - 1]["y"], (
                f"Cascade y not increasing: {wins[i-1]['y']} -> {wins[i]['y']}"
            )

    def test_arrange_changes_geometry(self, ipc, windows):
        """Verify that arranging windows actually changes their geometry."""
        windows.open_many(2, prefix="Change")
        time.sleep(SETTLE)

        # Record geometry before arrangement
        before = {}
        for i in range(2):
            win = ipc.get_window_by_title(f"Change{i+1}")
            assert win is not None
            before[f"Change{i+1}"] = {
                "x": win["x"], "y": win["y"],
                "width": win["width"], "height": win["height"],
            }

        ipc.command("ArrangeWindows")
        time.sleep(SETTLE)

        # At least one window should have changed geometry
        changed = False
        for i in range(2):
            title = f"Change{i+1}"
            win = ipc.get_window_by_title(title)
            assert win is not None
            orig = before[title]
            if (abs(win["x"] - orig["x"]) > TOLERANCE
                    or abs(win["y"] - orig["y"]) > TOLERANCE
                    or abs(win["width"] - orig["width"]) > TOLERANCE
                    or abs(win["height"] - orig["height"]) > TOLERANCE):
                changed = True
                break

        assert changed, "ArrangeWindows did not change any window's geometry"
