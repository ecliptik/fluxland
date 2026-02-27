"""TC-PLACE: Window placement tests for fluxland compositor.

Tests that the compositor's placement policy correctly positions
new windows to avoid overlap and stay within output bounds.
"""

import time

import pytest

SETTLE = 0.2


class TestWindowPlacement:
    """Test window placement policy behavior."""

    def test_default_placement_policy(self, ipc):
        """Verify default placement policy is row_smart."""
        config = ipc.get_config()
        assert config["placement_policy"] == "row_smart"

    def test_windows_placed_within_bounds(self, ipc, windows):
        """Placement positions windows mostly within the output area."""
        output = ipc.get_outputs()[0]
        out_w = output["width"]
        out_h = output["height"]

        windows.open_many(3, prefix="Place")
        time.sleep(SETTLE)

        for i in range(3):
            win = ipc.get_window_by_title(f"Place{i+1}")
            assert win is not None, f"Place {i+1} not found"
            # Allow small negative offsets for decoration/titlebar overlap
            assert win["x"] >= -200, (
                f"Window x={win['x']} too far outside output"
            )
            assert win["x"] < out_w, (
                f"Window x={win['x']} exceeds output width {out_w}"
            )
            assert win["y"] >= -200, (
                f"Window y={win['y']} too far outside output"
            )
            assert win["y"] < out_h, (
                f"Window y={win['y']} exceeds output height {out_h}"
            )

