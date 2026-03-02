"""Edge case integration tests for fluxland compositor.

Tests cover stress scenarios and robustness:
- Rapid window create/destroy cycles
- Config live-reload during active window operations
- IPC malformed input and error handling
"""

import json
import os
import socket
import time

import pytest

from lib.ipc_client import (
    FluxlandIPC,
    IPCError,
    ConnectionError as IPCConnectionError,
    CommandError,
    ProtocolError,
)


class TestRapidWindowLifecycle:
    """Stress tests for rapid window creation and destruction."""

    def test_rapid_open_close_single(self, ipc, windows):
        """Opening and immediately closing a window does not crash."""
        windows.open("RapidSingle")
        time.sleep(0.1)
        windows.close_all()
        time.sleep(0.3)

        # Compositor should still respond
        assert ipc.ping() is True
        win_list = ipc.get_windows()
        assert len(win_list) == 0

    def test_rapid_open_multiple(self, ipc, windows):
        """Opening 5 windows rapidly all appear in get_windows."""
        count = 5
        opened = windows.open_many(count, prefix="Rapid")
        assert len(opened) == count

        win_list = ipc.get_windows()
        titles = [w.get("title") for w in win_list]
        for i in range(1, count + 1):
            assert f"Rapid{i}" in titles

    def test_rapid_open_close_cycle(self, ipc, windows):
        """Multiple open/close cycles don't leave stale windows."""
        for cycle in range(3):
            title = f"Cycle{cycle}"
            windows.open(title)
            time.sleep(0.1)
            windows.close_all()
            time.sleep(0.3)

        # All windows should be gone
        assert ipc.ping() is True
        remaining = ipc.get_windows()
        assert len(remaining) == 0

    def test_compositor_stable_after_stress(self, ipc, windows):
        """After opening and closing many windows, compositor queries work."""
        for i in range(5):
            windows.open(f"Stress{i}", wait=True)

        windows.close_all()
        time.sleep(0.5)

        # All query types should still work
        assert ipc.ping() is True
        assert isinstance(ipc.get_windows(), list)
        assert isinstance(ipc.get_workspaces(), list)
        assert isinstance(ipc.get_outputs(), list)
        assert isinstance(ipc.get_config(), dict)


class TestConfigReload:
    """Tests for config live-reload during active operations."""

    def test_reconfigure_with_windows_open(self, ipc, windows):
        """Reconfigure command succeeds while windows are open."""
        windows.open("ReloadTest1")
        windows.open("ReloadTest2")
        time.sleep(0.2)

        # Trigger live reload
        result = ipc.command("Reconfigure")
        assert result.get("success") is True
        time.sleep(0.3)

        # Windows should still be present
        win_list = ipc.get_windows()
        titles = [w.get("title") for w in win_list]
        assert "ReloadTest1" in titles
        assert "ReloadTest2" in titles

    def test_reconfigure_preserves_workspace(self, ipc, windows):
        """Reconfigure does not change the active workspace."""
        initial_ws = ipc.get_active_workspace()
        initial_idx = initial_ws.get("index")

        result = ipc.command("Reconfigure")
        assert result.get("success") is True
        time.sleep(0.3)

        current_ws = ipc.get_active_workspace()
        assert current_ws.get("index") == initial_idx

    def test_reconfigure_twice_in_succession(self, ipc):
        """Two rapid Reconfigure commands both succeed."""
        r1 = ipc.command("Reconfigure")
        assert r1.get("success") is True

        r2 = ipc.command("Reconfigure")
        assert r2.get("success") is True
        time.sleep(0.3)

        assert ipc.ping() is True


class TestIPCEdgeCases:
    """IPC protocol edge cases and malformed input handling."""

    def test_empty_action_string(self, ipc):
        """Sending an empty action string returns an error."""
        with pytest.raises(CommandError):
            ipc.command("")

    def test_very_long_command(self, ipc):
        """Sending an extremely long command string is handled gracefully."""
        long_action = "A" * 10000
        with pytest.raises(CommandError):
            ipc.command(long_action)

    def test_command_with_special_characters(self, ipc):
        """Commands with special characters don't crash the compositor."""
        with pytest.raises(CommandError):
            ipc.command("Exec; rm -rf /")
        # Compositor must still respond
        assert ipc.ping() is True

    def test_malformed_json_raw_socket(self, compositor):
        """Sending malformed JSON to the IPC socket doesn't crash."""
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        try:
            sock.settimeout(5.0)
            sock.connect(compositor.socket_path)

            # Send invalid JSON
            sock.sendall(b"{{not valid json}}\n")
            time.sleep(0.2)

            # Read response — should be an error, not a crash
            data = b""
            try:
                data = sock.recv(4096)
            except socket.timeout:
                pass
        finally:
            sock.close()

        # Compositor must still respond via normal IPC
        client = FluxlandIPC(socket_path=compositor.socket_path)
        try:
            assert client.ping() is True
        finally:
            client.close()

    def test_incomplete_json_raw_socket(self, compositor):
        """Sending incomplete JSON to the IPC socket doesn't crash."""
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        try:
            sock.settimeout(2.0)
            sock.connect(compositor.socket_path)

            # Send incomplete JSON (no newline terminator, then close)
            sock.sendall(b'{"command": "ping"')
            time.sleep(0.2)
        finally:
            sock.close()

        # Compositor must still respond
        client = FluxlandIPC(socket_path=compositor.socket_path)
        try:
            assert client.ping() is True
        finally:
            client.close()

    def test_null_bytes_in_command(self, compositor):
        """Null bytes in IPC messages don't crash the compositor."""
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        try:
            sock.settimeout(2.0)
            sock.connect(compositor.socket_path)
            sock.sendall(b'{"command": "ping\x00"}\n')
            time.sleep(0.2)
            try:
                sock.recv(4096)
            except socket.timeout:
                pass
        finally:
            sock.close()

        client = FluxlandIPC(socket_path=compositor.socket_path)
        try:
            assert client.ping() is True
        finally:
            client.close()

    def test_rapid_connect_disconnect(self, compositor):
        """Rapidly connecting and disconnecting doesn't crash."""
        for _ in range(10):
            sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            try:
                sock.settimeout(2.0)
                sock.connect(compositor.socket_path)
            finally:
                sock.close()

        # Compositor must still respond
        client = FluxlandIPC(socket_path=compositor.socket_path)
        try:
            assert client.ping() is True
        finally:
            client.close()

    def test_sequential_different_commands(self, ipc):
        """Mixing different IPC command types in sequence works."""
        assert ipc.ping() is True
        assert isinstance(ipc.get_windows(), list)
        assert isinstance(ipc.get_workspaces(), list)
        assert isinstance(ipc.get_outputs(), list)
        assert isinstance(ipc.get_config(), dict)
        assert ipc.ping() is True


class TestWorkspaceEdgeCases:
    """Edge cases for workspace operations."""

    def test_switch_to_current_workspace(self, ipc):
        """Switching to the already-active workspace is a no-op."""
        ws = ipc.get_active_workspace()
        idx = ws.get("index")

        result = ipc.command(f"Workspace {idx}")
        assert result.get("success") is True

        ws_after = ipc.get_active_workspace()
        assert ws_after.get("index") == idx

    def test_add_remove_workspace_cycle(self, ipc):
        """Adding and removing workspaces in a cycle preserves count."""
        initial = ipc.get_workspaces()
        initial_count = len(initial)

        for _ in range(3):
            ipc.command("AddWorkspace")
            time.sleep(0.1)

        mid = ipc.get_workspaces()
        assert len(mid) == initial_count + 3

        for _ in range(3):
            ipc.command("RemoveLastWorkspace")
            time.sleep(0.1)

        final = ipc.get_workspaces()
        assert len(final) == initial_count
