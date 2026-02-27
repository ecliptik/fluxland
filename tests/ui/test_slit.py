"""Tests for slit/dock operations via IPC."""

import time

import pytest

SETTLE = 0.2


class TestSlit:
    """Test slit toggle operations via IPC.

    The slit may not exist in headless mode (no slit clients), but
    toggle commands return success regardless since they check
    if (server->slit) before operating.
    """

    def test_toggle_slit_above_succeeds(self, ipc):
        """ToggleSlitAbove returns success."""
        try:
            result = ipc.command("ToggleSlitAbove")
            time.sleep(SETTLE)
            assert result.get("success") is True
        finally:
            ipc.command("ToggleSlitAbove")
            time.sleep(SETTLE)

    def test_toggle_slit_hidden_succeeds(self, ipc):
        """ToggleSlitHidden returns success."""
        try:
            result = ipc.command("ToggleSlitHidden")
            time.sleep(SETTLE)
            assert result.get("success") is True
        finally:
            ipc.command("ToggleSlitHidden")
            time.sleep(SETTLE)

    def test_toggle_slit_twice_is_idempotent(self, ipc):
        """ToggleSlitAbove twice returns success both times."""
        result1 = ipc.command("ToggleSlitAbove")
        time.sleep(SETTLE)
        assert result1.get("success") is True

        result2 = ipc.command("ToggleSlitAbove")
        time.sleep(SETTLE)
        assert result2.get("success") is True
