"""Keybinding operation tests for fluxland compositor.

Tests verify that BindKey IPC commands are accepted by the compositor
and return success. Actual key press simulation requires wtype which
may not be available in headless mode.
"""

import time

import pytest

SETTLE = 0.2


class TestKeybindingOperations:
    """Test keybinding IPC commands."""

    def test_bind_key_succeeds(self, ipc):
        """BindKey with valid arguments returns success."""
        result = ipc.command("BindKey Mod4 t :Exec foot")
        assert result.get("success") is True

    def test_bind_key_mod4(self, ipc):
        """BindKey with Mod4 modifier succeeds."""
        result = ipc.command("BindKey Mod4 x :Exec echo test")
        assert result.get("success") is True

    def test_bind_key_control(self, ipc):
        """BindKey with Control modifier succeeds."""
        result = ipc.command("BindKey Control t :Exec echo test")
        assert result.get("success") is True

    def test_bind_key_shift(self, ipc):
        """BindKey with Shift modifier succeeds."""
        result = ipc.command("BindKey Shift F1 :Exec echo test")
        assert result.get("success") is True

    def test_bind_key_multi_mod(self, ipc):
        """BindKey with multiple modifiers succeeds."""
        result = ipc.command("BindKey Mod4 Shift q :Exit")
        assert result.get("success") is True

    def test_bind_key_maximize(self, ipc):
        """BindKey bound to Maximize action succeeds."""
        result = ipc.command("BindKey Mod4 m :Maximize")
        assert result.get("success") is True

    def test_bind_key_workspace(self, ipc):
        """BindKey bound to Workspace action succeeds."""
        result = ipc.command("BindKey Mod4 1 :Workspace 1")
        assert result.get("success") is True

    def test_bind_key_close(self, ipc):
        """BindKey bound to Close action succeeds."""
        result = ipc.command("BindKey Mod4 w :Close")
        assert result.get("success") is True

    def test_bind_key_no_args_succeeds(self, ipc):
        """BindKey with no arguments returns success (no-op)."""
        result = ipc.command("BindKey")
        assert result.get("success") is True

    def test_bind_key_action_only(self, ipc):
        """BindKey with Mod4 Return bound to Exec foot succeeds."""
        result = ipc.command("BindKey Mod4 Return :Exec foot")
        assert result.get("success") is True
