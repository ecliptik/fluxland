"""Tab group operation tests for fluxland compositor.

Tab groups are formed via mouse-initiated StartTabbing, which is not
available in headless mode. These tests verify that tab-related IPC
commands return success and do not crash the compositor. Without an
active tab group, commands like NextTab/PrevTab are no-ops.
"""

import time

import pytest

SETTLE = 0.2


class TestTabOperations:
    """Test tab group IPC commands."""

    def test_start_tabbing_succeeds(self, ipc):
        """StartTabbing returns success (no-op via IPC, requires mouse)."""
        result = ipc.command("StartTabbing")
        assert result.get("success") is True

    def test_next_tab_succeeds_single_window(self, ipc, windows):
        """NextTab on a single window returns success (no-op, no tab group)."""
        windows.open("TabNext")
        time.sleep(SETTLE)

        result = ipc.command("NextTab")
        assert result.get("success") is True

    def test_prev_tab_succeeds_single_window(self, ipc, windows):
        """PrevTab on a single window returns success (no-op, no tab group)."""
        windows.open("TabPrev")
        time.sleep(SETTLE)

        result = ipc.command("PrevTab")
        assert result.get("success") is True

    def test_detach_client_succeeds(self, ipc, windows):
        """DetachClient returns success (no-op without tab group)."""
        windows.open("TabDetach")
        time.sleep(SETTLE)

        result = ipc.command("DetachClient")
        assert result.get("success") is True

    def test_move_tab_left_succeeds(self, ipc, windows):
        """MoveTabLeft returns success (no-op without tab group)."""
        windows.open("TabMoveL")
        time.sleep(SETTLE)

        result = ipc.command("MoveTabLeft")
        assert result.get("success") is True

    def test_activate_tab_succeeds(self, ipc, windows):
        """ActivateTab 0 returns success (no-op without tab group)."""
        windows.open("TabAct")
        time.sleep(SETTLE)

        result = ipc.command("ActivateTab 0")
        assert result.get("success") is True
