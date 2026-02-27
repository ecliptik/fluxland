"""IPC protocol tests for fluxland compositor.

Tests cover the full IPC client API: queries, commands, error handling,
event subscriptions, and sequential/rapid operation patterns.
"""

import time

import pytest

from lib.ipc_client import (
    FluxlandIPC,
    IPCError,
    ConnectionError as IPCConnectionError,
    CommandError,
)


class TestIPCProtocol:
    """Core IPC protocol operations."""

    def test_ping(self, ipc):
        """ping() returns True when compositor is running."""
        assert ipc.ping() is True

    def test_get_workspaces_returns_list(self, ipc):
        """get_workspaces() returns a list."""
        workspaces = ipc.get_workspaces()
        assert isinstance(workspaces, list)

    def test_get_windows_empty(self, ipc):
        """With no windows open, get_windows() returns an empty list."""
        # Wait for any lingering windows from prior tests to close
        deadline = time.monotonic() + 2.0
        while time.monotonic() < deadline:
            windows = ipc.get_windows()
            if len(windows) == 0:
                break
            time.sleep(0.2)
        assert isinstance(windows, list)
        assert len(windows) == 0, (
            f"Expected 0 windows but found {len(windows)}: "
            f"{[w.get('title') for w in windows]}"
        )

    def test_get_windows_reflects_open(self, ipc, windows):
        """Opening a window makes it appear in get_windows() with correct title."""
        title = "IPC-Test-Open"
        windows.open(title)
        time.sleep(0.2)

        win_list = ipc.get_windows()
        titles = [w.get("title") for w in win_list]
        assert title in titles

    def test_get_windows_reflects_close(self, ipc, windows):
        """After opening and closing a window, it is removed from get_windows()."""
        title = "IPC-Test-Close"
        windows.open(title)
        time.sleep(0.2)

        # Verify it appeared
        assert ipc.get_window_by_title(title) is not None

        # Close all windows and verify it's gone
        windows.close_all()
        time.sleep(0.3)

        assert ipc.get_window_by_title(title) is None

    def test_get_outputs_has_geometry(self, ipc):
        """At least one output exists with positive width and height."""
        outputs = ipc.get_outputs()
        assert isinstance(outputs, list)
        assert len(outputs) >= 1

        out = outputs[0]
        assert out["width"] > 0
        assert out["height"] > 0

    def test_get_config_has_keys(self, ipc):
        """get_config() returns expected configuration keys."""
        config = ipc.get_config()
        assert isinstance(config, dict)
        for key in ("workspace_count", "focus_policy"):
            assert key in config, f"Missing config key: {key}"

    def test_command_returns_success(self, ipc):
        """command('AddWorkspace') succeeds and workspace count increases."""
        initial = ipc.get_workspaces()
        initial_count = len(initial)

        result = ipc.command("AddWorkspace")
        assert result.get("success") is True
        time.sleep(0.1)

        updated = ipc.get_workspaces()
        assert len(updated) == initial_count + 1

        # Clean up
        ipc.command("RemoveLastWorkspace")
        time.sleep(0.1)


class TestIPCErrors:
    """IPC error handling."""

    def test_unknown_command_error(self, ipc):
        """Sending a bogus command raises CommandError."""
        with pytest.raises(CommandError):
            ipc.command("TotallyBogusAction12345")

    def test_connection_error_bad_path(self):
        """Connecting to a nonexistent socket raises IPCConnectionError."""
        with pytest.raises(IPCConnectionError):
            FluxlandIPC(socket_path="/tmp/nonexistent-fluxland-test.sock")


class TestIPCEvents:
    """IPC event subscription."""

    def test_subscribe_window_events(self, ipc, windows):
        """Subscribing to window events captures a window_open event."""
        # Subscribe BEFORE opening the window
        stream = ipc.subscribe(["window"])
        try:
            windows.open("IPC-Event-Test")
            time.sleep(0.2)

            event = stream.wait_for("window_open", timeout=5.0)
            assert event is not None
            assert event.get("event") == "window_open"
        finally:
            stream.close()


class TestIPCSequential:
    """Sequential and rapid IPC operations."""

    def test_multiple_commands(self, ipc):
        """AddWorkspace, get_workspaces, RemoveLastWorkspace all succeed in sequence."""
        # AddWorkspace
        result = ipc.command("AddWorkspace")
        assert result.get("success") is True
        time.sleep(0.1)

        # get_workspaces
        workspaces = ipc.get_workspaces()
        assert isinstance(workspaces, list)
        assert len(workspaces) >= 2

        # RemoveLastWorkspace
        result = ipc.command("RemoveLastWorkspace")
        assert result.get("success") is True
        time.sleep(0.1)

    def test_rapid_queries(self, ipc):
        """Calling get_windows() 10 times rapidly all return valid lists."""
        for _ in range(10):
            result = ipc.get_windows()
            assert isinstance(result, list)
