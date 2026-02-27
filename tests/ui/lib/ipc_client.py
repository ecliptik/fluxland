"""Fluxland IPC client library.

Provides a Python interface to the fluxland compositor's Unix domain socket
IPC protocol. Supports querying compositor state, executing commands, and
subscribing to real-time events.

Protocol details:
    - Transport: Unix domain socket (SOCK_STREAM)
    - Socket path: $FLUXLAND_SOCK or $XDG_RUNTIME_DIR/fluxland.$WAYLAND_DISPLAY.sock
    - Framing: Newline-delimited JSON
    - Security: SO_PEERCRED UID check (same user only)

Example usage::

    with FluxlandIPC() as ipc:
        windows = ipc.get_windows()
        focused = ipc.get_focused_window()
        ipc.command("Maximize")
"""

import json
import os
import select
import socket
import time


class IPCError(Exception):
    """Base exception for IPC errors."""


class ConnectionError(IPCError):
    """Failed to connect to the compositor."""


class TimeoutError(IPCError):
    """Operation timed out."""


class ProtocolError(IPCError):
    """Malformed response from the compositor."""


class CommandError(IPCError):
    """Compositor returned an error for a command."""


def _discover_socket_path():
    """Discover the fluxland IPC socket path.

    Checks, in order:
        1. $FLUXLAND_SOCK environment variable
        2. $XDG_RUNTIME_DIR/fluxland.$WAYLAND_DISPLAY.sock

    Returns:
        The socket path string.

    Raises:
        ConnectionError: If the socket path cannot be determined.
    """
    sock_path = os.environ.get("FLUXLAND_SOCK")
    if sock_path:
        return sock_path

    runtime_dir = os.environ.get("XDG_RUNTIME_DIR")
    if not runtime_dir:
        raise ConnectionError(
            "Cannot determine socket path: "
            "neither FLUXLAND_SOCK nor XDG_RUNTIME_DIR is set"
        )

    wayland_display = os.environ.get("WAYLAND_DISPLAY", "wayland-0")
    return os.path.join(runtime_dir, f"fluxland.{wayland_display}.sock")


def _connect(socket_path, timeout):
    """Create and connect a Unix domain socket.

    Args:
        socket_path: Path to the Unix domain socket.
        timeout: Connection timeout in seconds.

    Returns:
        A connected socket object.

    Raises:
        ConnectionError: If the connection fails.
    """
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    sock.settimeout(timeout)
    try:
        sock.connect(socket_path)
    except FileNotFoundError:
        sock.close()
        raise ConnectionError(f"Socket not found: {socket_path}")
    except PermissionError:
        sock.close()
        raise ConnectionError(f"Permission denied: {socket_path}")
    except socket.timeout:
        sock.close()
        raise TimeoutError(f"Connection timed out: {socket_path}")
    except OSError as e:
        sock.close()
        raise ConnectionError(f"Connection failed: {socket_path}: {e}")
    return sock


class EventStream:
    """Iterator over IPC event notifications.

    Uses a dedicated socket connection so event streaming does not
    block command/query operations on the main FluxlandIPC socket.

    The stream is iterable and supports waiting for specific event types
    with a timeout.

    Example::

        with ipc.subscribe(["window", "workspace"]) as events:
            for event in events:
                print(event)

        # Or wait for a specific event:
        with ipc.subscribe(["window"]) as events:
            evt = events.wait_for("window_open", timeout=10.0)
    """

    def __init__(self, sock):
        """Initialize the event stream.

        Args:
            sock: A connected socket already subscribed to events.
        """
        self._sock = sock
        self._buffer = b""
        self._closed = False

    def close(self):
        """Close the event stream and its underlying socket."""
        if not self._closed:
            self._closed = True
            try:
                self._sock.close()
            except OSError:
                pass

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()
        return False

    def __iter__(self):
        return self

    def __next__(self):
        """Return the next event, blocking until one arrives.

        Returns:
            A dict representing the event.

        Raises:
            StopIteration: If the connection is closed.
            ProtocolError: If a malformed event is received.
        """
        event = self._read_event(timeout=None)
        if event is None:
            raise StopIteration
        return event

    def _read_event(self, timeout=None):
        """Read a single event from the socket.

        Args:
            timeout: Maximum time to wait in seconds, or None to block
                indefinitely.

        Returns:
            A dict representing the event, or None if the connection closed
            or the timeout expired.

        Raises:
            ProtocolError: If a malformed event is received.
        """
        if self._closed:
            return None

        deadline = None
        if timeout is not None:
            deadline = time.monotonic() + timeout

        while True:
            # Check buffer for a complete line
            newline_pos = self._buffer.find(b"\n")
            if newline_pos >= 0:
                line = self._buffer[:newline_pos]
                self._buffer = self._buffer[newline_pos + 1:]
                if not line:
                    continue
                try:
                    return json.loads(line)
                except json.JSONDecodeError as e:
                    raise ProtocolError(f"Malformed event JSON: {e}")

            # Wait for more data
            if deadline is not None:
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    return None
                wait_timeout = remaining
            else:
                wait_timeout = None

            try:
                ready, _, _ = select.select(
                    [self._sock], [], [], wait_timeout
                )
            except (OSError, ValueError):
                self._closed = True
                return None

            if not ready:
                return None  # Timeout

            try:
                data = self._sock.recv(4096)
            except OSError:
                self._closed = True
                return None

            if not data:
                self._closed = True
                return None

            self._buffer += data

    def wait_for(self, event_type, timeout=5.0):
        """Wait for a specific event type.

        Reads events until one matching ``event_type`` arrives or the
        timeout expires. Non-matching events are discarded.

        Args:
            event_type: The event type string to wait for (e.g.
                ``"window_open"``).
            timeout: Maximum time to wait in seconds.

        Returns:
            The matching event dict.

        Raises:
            TimeoutError: If no matching event arrives within the timeout.
            ProtocolError: If a malformed event is received.
        """
        deadline = time.monotonic() + timeout
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError(
                    f"Timed out waiting for event '{event_type}'"
                )
            event = self._read_event(timeout=remaining)
            if event is None:
                raise TimeoutError(
                    f"Timed out waiting for event '{event_type}'"
                )
            if event.get("event") == event_type:
                return event


class FluxlandIPC:
    """Client for the fluxland compositor IPC protocol.

    Connects to the compositor's Unix domain socket and provides methods
    for querying state, executing commands, and subscribing to events.

    Can be used as a context manager::

        with FluxlandIPC() as ipc:
            print(ipc.get_windows())

    Args:
        socket_path: Path to the IPC socket. If None, auto-discovers
            from environment variables.
        timeout: Default timeout in seconds for socket operations.
    """

    def __init__(self, socket_path=None, timeout=5.0):
        self._timeout = timeout
        self._socket_path = socket_path or _discover_socket_path()
        self._sock = _connect(self._socket_path, timeout)
        self._sock.settimeout(timeout)
        self._buffer = b""

    def close(self):
        """Close the IPC connection."""
        if self._sock is not None:
            try:
                self._sock.close()
            except OSError:
                pass
            self._sock = None

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()
        return False

    def _send(self, request):
        """Send a JSON request and read the JSON response.

        Args:
            request: A dict to serialize as JSON and send.

        Returns:
            The parsed response dict.

        Raises:
            ConnectionError: If the socket is closed.
            TimeoutError: If the response is not received in time.
            ProtocolError: If the response is malformed.
        """
        if self._sock is None:
            raise ConnectionError("Not connected")

        payload = json.dumps(request, separators=(",", ":")) + "\n"
        try:
            self._sock.sendall(payload.encode("utf-8"))
        except BrokenPipeError:
            raise ConnectionError("Connection closed by compositor")
        except socket.timeout:
            raise TimeoutError("Send timed out")
        except OSError as e:
            raise ConnectionError(f"Send failed: {e}")

        return self._read_response()

    def _read_response(self):
        """Read a single newline-delimited JSON response.

        Returns:
            The parsed response dict.

        Raises:
            TimeoutError: If no complete response arrives in time.
            ProtocolError: If the response is malformed.
            ConnectionError: If the connection is closed.
        """
        deadline = time.monotonic() + self._timeout

        while True:
            # Check buffer for a complete line
            newline_pos = self._buffer.find(b"\n")
            if newline_pos >= 0:
                line = self._buffer[:newline_pos]
                self._buffer = self._buffer[newline_pos + 1:]
                if not line:
                    continue
                try:
                    return json.loads(line)
                except json.JSONDecodeError as e:
                    raise ProtocolError(f"Malformed response JSON: {e}")

            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError("Response timed out")

            try:
                ready, _, _ = select.select(
                    [self._sock], [], [], remaining
                )
            except (OSError, ValueError):
                raise ConnectionError("Connection lost during read")

            if not ready:
                raise TimeoutError("Response timed out")

            try:
                data = self._sock.recv(4096)
            except socket.timeout:
                raise TimeoutError("Response timed out")
            except OSError as e:
                raise ConnectionError(f"Read failed: {e}")

            if not data:
                raise ConnectionError("Connection closed by compositor")

            self._buffer += data

    def _check_response(self, response):
        """Validate a response and raise on error.

        Args:
            response: The parsed response dict.

        Returns:
            The response dict (unchanged).

        Raises:
            CommandError: If the response indicates failure.
            ProtocolError: If the response is not a dict.
        """
        if not isinstance(response, dict):
            raise ProtocolError(f"Expected dict response, got {type(response).__name__}")
        if not response.get("success"):
            error_msg = response.get("error", "unknown error")
            raise CommandError(error_msg)
        return response

    # -- Query methods --

    def ping(self):
        """Ping the compositor.

        Returns:
            True if the compositor responds successfully.

        Raises:
            IPCError: On connection, timeout, or protocol errors.
        """
        response = self._send({"command": "ping"})
        self._check_response(response)
        return True

    def get_windows(self):
        """Get the list of all windows.

        Returns:
            A list of window dicts, each containing keys like ``app_id``,
            ``title``, ``workspace``, ``focused``, ``maximized``,
            ``fullscreen``, ``sticky``, ``x``, ``y``, ``width``, ``height``.
        """
        response = self._send({"command": "get_windows"})
        self._check_response(response)
        return response.get("data", [])

    def get_workspaces(self):
        """Get the list of all workspaces.

        Returns:
            A list of workspace dicts, each containing keys like ``index``,
            ``name``, ``focused``, ``visible``.
        """
        response = self._send({"command": "get_workspaces"})
        self._check_response(response)
        return response.get("data", [])

    def get_outputs(self):
        """Get the list of all outputs (monitors).

        Returns:
            A list of output dicts, each containing keys like ``name``,
            ``width``, ``height``, ``x``, ``y``.
        """
        response = self._send({"command": "get_outputs"})
        self._check_response(response)
        return response.get("data", [])

    def get_config(self):
        """Get the current compositor configuration.

        Returns:
            A dict with configuration keys like ``workspace_count``,
            ``focus_policy``, ``placement_policy``, ``edge_snap_threshold``,
            ``toolbar_visible``.
        """
        response = self._send({"command": "get_config"})
        self._check_response(response)
        return response.get("data", {})

    # -- Command execution --

    def command(self, action):
        """Execute a compositor command.

        Args:
            action: The action string (e.g. ``"Maximize"``,
                ``"Workspace 2"``, ``"MoveTo 100 200"``).

        Returns:
            The response dict.

        Raises:
            CommandError: If the command fails.
        """
        response = self._send({"command": "command", "action": action})
        self._check_response(response)
        return response

    # -- Event subscription --

    def subscribe(self, events):
        """Subscribe to compositor events on a separate connection.

        Creates a new socket connection dedicated to receiving events,
        so that event streaming does not block the main command socket.

        Args:
            events: A list of event type strings (e.g.
                ``["window", "workspace"]``). Valid types: ``window``,
                ``window_close``, ``window_focus``, ``window_title``,
                ``workspace``, ``output``, ``style_changed``,
                ``config_reloaded``, ``focus_changed``, ``menu``,
                ``accessibility``.

        Returns:
            An :class:`EventStream` that yields event dicts.

        Raises:
            CommandError: If the subscribe request fails.
        """
        event_sock = _connect(self._socket_path, self._timeout)
        event_sock.settimeout(self._timeout)

        events_str = ",".join(events)
        payload = (
            json.dumps(
                {"command": "subscribe", "events": events_str},
                separators=(",", ":"),
            )
            + "\n"
        )

        try:
            event_sock.sendall(payload.encode("utf-8"))
        except OSError as e:
            event_sock.close()
            raise ConnectionError(f"Failed to send subscribe request: {e}")

        # Read the subscribe acknowledgment
        buf = b""
        deadline = time.monotonic() + self._timeout
        while True:
            newline_pos = buf.find(b"\n")
            if newline_pos >= 0:
                line = buf[:newline_pos]
                # Any remaining data after the ack is the start of events
                remainder = buf[newline_pos + 1:]
                break

            remaining = deadline - time.monotonic()
            if remaining <= 0:
                event_sock.close()
                raise TimeoutError("Subscribe acknowledgment timed out")

            ready, _, _ = select.select([event_sock], [], [], remaining)
            if not ready:
                event_sock.close()
                raise TimeoutError("Subscribe acknowledgment timed out")

            data = event_sock.recv(4096)
            if not data:
                event_sock.close()
                raise ConnectionError("Connection closed during subscribe")
            buf += data

        try:
            ack = json.loads(line)
        except json.JSONDecodeError as e:
            event_sock.close()
            raise ProtocolError(f"Malformed subscribe response: {e}")

        if not ack.get("success"):
            event_sock.close()
            error_msg = ack.get("error", "unknown error")
            raise CommandError(f"Subscribe failed: {error_msg}")

        # Switch to non-blocking for the event stream
        event_sock.setblocking(False)

        stream = EventStream(event_sock)
        stream._buffer = remainder
        return stream

    def wait_for_event(self, event_type, timeout=5.0, events=None):
        """Subscribe and wait for a single event.

        Convenience method that creates a temporary subscription, waits
        for the specified event type, and cleans up.

        Args:
            event_type: The event type string to wait for.
            timeout: Maximum time to wait in seconds.
            events: Event types to subscribe to. If None, subscribes to
                a reasonable default based on ``event_type``.

        Returns:
            The matching event dict.

        Raises:
            TimeoutError: If no matching event arrives in time.
        """
        if events is None:
            # Map specific event types to their subscription categories
            category_map = {
                "window_open": "window",
                "window_close": "window",
                "window_focus": "window",
                "window_title": "window",
                "output_add": "output",
                "output_remove": "output",
            }
            category = category_map.get(event_type, event_type)
            events = [category]

        with self.subscribe(events) as stream:
            return stream.wait_for(event_type, timeout=timeout)

    # -- Convenience wrappers --

    def get_focused_window(self):
        """Get the currently focused window.

        Returns:
            The focused window dict, or None if no window is focused.
        """
        windows = self.get_windows()
        for win in windows:
            if win.get("focused"):
                return win
        return None

    def get_window_by_title(self, title):
        """Find a window by its title (exact match).

        Args:
            title: The window title to search for.

        Returns:
            The first matching window dict, or None.
        """
        windows = self.get_windows()
        for win in windows:
            if win.get("title") == title:
                return win
        return None

    def get_window_by_app_id(self, app_id):
        """Find a window by its app_id (exact match).

        Args:
            app_id: The application ID to search for.

        Returns:
            The first matching window dict, or None.
        """
        windows = self.get_windows()
        for win in windows:
            if win.get("app_id") == app_id:
                return win
        return None

    def get_active_workspace(self):
        """Get the currently active (focused) workspace.

        Returns:
            The active workspace dict.

        Raises:
            IPCError: If no active workspace is found.
        """
        workspaces = self.get_workspaces()
        for ws in workspaces:
            if ws.get("focused"):
                return ws
        raise IPCError("No active workspace found")
