"""TC-LAYERS: Window layer operation tests for fluxland compositor.

Tests SetLayer, RaiseLayer, and LowerLayer commands which control
the stacking layer of windows.
"""

import time

import pytest


SETTLE = 0.2  # seconds to wait after commands for state to settle


class TestLayers:
    """Verify window layer commands via IPC."""

    def test_set_layer_above_succeeds(self, ipc, windows):
        """SetLayer above returns success."""
        windows.open("LayerAbove")
        time.sleep(SETTLE)

        result = ipc.command("SetLayer above")
        time.sleep(SETTLE)

        assert result["success"] is True

    def test_set_layer_normal_succeeds(self, ipc, windows):
        """SetLayer normal returns success."""
        windows.open("LayerNormal")
        time.sleep(SETTLE)

        result = ipc.command("SetLayer normal")
        time.sleep(SETTLE)

        assert result["success"] is True

    def test_raise_layer_succeeds(self, ipc, windows):
        """RaiseLayer returns success."""
        windows.open("RaiseLayer")
        time.sleep(SETTLE)

        result = ipc.command("RaiseLayer")
        time.sleep(SETTLE)

        assert result["success"] is True

    def test_lower_layer_succeeds(self, ipc, windows):
        """LowerLayer returns success."""
        windows.open("LowerLayer")
        time.sleep(SETTLE)

        result = ipc.command("LowerLayer")
        time.sleep(SETTLE)

        assert result["success"] is True
