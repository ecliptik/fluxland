"""Compositor lifecycle management for fluxland UI tests.

Handles starting/stopping the fluxland compositor in both headless mode
(for CI) and live session mode (connecting to an already-running compositor).

IPC protocol: newline-delimited JSON over Unix domain socket.
Socket path: $XDG_RUNTIME_DIR/fluxland.$WAYLAND_DISPLAY.sock
"""

import glob
import json
import os
import shutil
import signal
import socket
import subprocess
import tempfile
import time
from pathlib import Path


class CompositorError(Exception):
    """Raised when compositor operations fail."""


class CompositorManager:
    """Manage fluxland compositor lifecycle for testing.

    Supports two modes:
    - headless: Launches a new compositor with WLR_BACKENDS=headless
    - live session: Connects to the compositor already running on WAYLAND_DISPLAY
    """

    def __init__(self, binary_path="fluxland", config_base_dir=None):
        """Initialize compositor manager.

        Args:
            binary_path: Path to the fluxland binary, or just "fluxland"
                to search PATH. Also checks build directory.
            config_base_dir: Directory containing config sets (subdirs like
                "default/"). Defaults to tests/ui/configs relative to this file.
        """
        self._binary_path = self._resolve_binary(binary_path)
        if config_base_dir is None:
            config_base_dir = str(
                Path(__file__).parent.parent / "configs"
            )
        self._config_base_dir = config_base_dir

        self._process = None
        self._tmp_dir = None
        self._runtime_dir = None
        self._config_dir = None
        self._socket_path = None
        self._wayland_display = None
        self._is_live_session = False
        self._original_env = {}

    @staticmethod
    def _resolve_binary(binary_path):
        """Find the fluxland binary, checking common build locations."""
        if os.path.isabs(binary_path) and os.access(binary_path, os.X_OK):
            return binary_path

        # Check build directory relative to project root
        project_root = Path(__file__).parent.parent.parent.parent
        build_bin = project_root / "build" / "fluxland"
        if build_bin.exists() and os.access(str(build_bin), os.X_OK):
            return str(build_bin)

        # Check test build location used by integration harness
        test_bin = Path("/tmp/fluxland-test/wm-build/fluxland")
        if test_bin.exists() and os.access(str(test_bin), os.X_OK):
            return str(test_bin)

        # Fall back to PATH lookup
        which = shutil.which(binary_path)
        if which:
            return which

        return binary_path

    def start(self, config_set="default", headless=False):
        """Start or connect to the fluxland compositor.

        Args:
            config_set: Name of the config set directory under config_base_dir.
            headless: If True, launch a new headless compositor.
                If False, connect to the existing session on WAYLAND_DISPLAY.

        Returns:
            subprocess.Popen for headless mode, None for live session mode.

        Raises:
            CompositorError: If the compositor fails to start or connect.
        """
        if headless:
            return self._start_headless(config_set)
        else:
            return self._connect_live_session()

    def _start_headless(self, config_set):
        """Launch a new headless compositor instance."""
        # Create temp directory structure matching integration harness
        self._tmp_dir = tempfile.mkdtemp(
            prefix=f"fluxland-uitest-{os.getpid()}-"
        )
        self._runtime_dir = os.path.join(self._tmp_dir, "runtime")
        self._config_dir = os.path.join(self._tmp_dir, "config", "fluxland")

        os.makedirs(self._runtime_dir, mode=0o700)
        os.makedirs(self._config_dir, mode=0o755)

        # Copy config files
        self.set_config(config_set)

        # Build environment for compositor
        env = os.environ.copy()
        env["WLR_BACKENDS"] = "headless"
        env["WLR_RENDERER"] = "pixman"
        env["XDG_RUNTIME_DIR"] = self._runtime_dir
        env["XDG_CONFIG_HOME"] = os.path.join(self._tmp_dir, "config")
        env["HOME"] = self._tmp_dir

        # Launch compositor
        self._process = subprocess.Popen(
            [self._binary_path],
            env=env,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE,
        )

        # Wait for Wayland socket to appear
        self._wayland_display = self._wait_for_wayland_socket(timeout=5.0)
        if not self._wayland_display:
            stderr_output = ""
            if self._process.poll() is not None:
                stderr_output = self._process.stderr.read().decode(
                    errors="replace"
                )
            self.stop()
            raise CompositorError(
                f"Compositor failed to create Wayland socket. "
                f"stderr: {stderr_output}"
            )

        # Build IPC socket path
        self._socket_path = os.path.join(
            self._runtime_dir,
            f"fluxland.{self._wayland_display}.sock",
        )

        # Wait for IPC to be ready
        if not self.wait_ready(timeout=5.0):
            self.stop()
            raise CompositorError("Compositor IPC socket not responding")

        self._is_live_session = False
        return self._process

    def _connect_live_session(self):
        """Connect to an already-running compositor on the current display."""
        wayland_display = os.environ.get("WAYLAND_DISPLAY")
        if not wayland_display:
            raise CompositorError(
                "WAYLAND_DISPLAY not set; cannot connect to live session"
            )

        runtime_dir = os.environ.get("XDG_RUNTIME_DIR")
        if not runtime_dir:
            raise CompositorError(
                "XDG_RUNTIME_DIR not set; cannot locate IPC socket"
            )

        # Check for FLUXLAND_SOCK env var first (set by compositor)
        fluxland_sock = os.environ.get("FLUXLAND_SOCK")
        if fluxland_sock and os.path.exists(fluxland_sock):
            self._socket_path = fluxland_sock
        else:
            self._socket_path = os.path.join(
                runtime_dir, f"fluxland.{wayland_display}.sock"
            )

        if not os.path.exists(self._socket_path):
            raise CompositorError(
                f"IPC socket not found: {self._socket_path}"
            )

        self._runtime_dir = runtime_dir
        self._wayland_display = wayland_display
        self._is_live_session = True

        # Verify connectivity
        if not self.wait_ready(timeout=3.0):
            raise CompositorError(
                f"Cannot connect to IPC socket: {self._socket_path}"
            )

        return None

    def _wait_for_wayland_socket(self, timeout=5.0):
        """Poll for the Wayland socket file to appear in runtime_dir.

        Returns the socket name (e.g. 'wayland-0') or None on timeout.
        """
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            # Check if process died
            if self._process and self._process.poll() is not None:
                return None
            # Scan for wayland-N socket files
            for i in range(10):
                sock_path = os.path.join(
                    self._runtime_dir, f"wayland-{i}"
                )
                if os.path.exists(sock_path):
                    return f"wayland-{i}"
            time.sleep(0.05)
        return None

    def stop(self, timeout=5.0):
        """Stop the compositor gracefully.

        For headless mode:
            1. Try IPC Exit command
            2. If still running after 2s, send SIGTERM
            3. If still running after timeout, send SIGKILL

        For live session mode: does nothing (we don't own the compositor).
        """
        if self._is_live_session:
            self._socket_path = None
            self._runtime_dir = None
            return

        if self._process and self._process.poll() is None:
            # Step 1: Try graceful IPC exit
            try:
                self._ipc_send({"command": "command", "action": "Exit"})
            except (OSError, CompositorError):
                pass

            # Step 2: Wait up to 2s for graceful exit
            try:
                self._process.wait(timeout=2.0)
            except subprocess.TimeoutExpired:
                # Step 3: SIGTERM
                self._process.terminate()
                try:
                    self._process.wait(timeout=timeout - 2.0)
                except subprocess.TimeoutExpired:
                    # Step 4: SIGKILL
                    self._process.kill()
                    self._process.wait(timeout=1.0)

        self._process = None
        self._socket_path = None

        # Clean up temp directory
        if self._tmp_dir and os.path.exists(self._tmp_dir):
            shutil.rmtree(self._tmp_dir, ignore_errors=True)
            self._tmp_dir = None

    def reconfigure(self):
        """Send Reconfigure command via IPC.

        Raises:
            CompositorError: If the command fails.
        """
        response = self._ipc_command("Reconfigure")
        return response

    def set_config(self, config_set):
        """Copy config files from configs/<set>/ to runtime config dir.

        Args:
            config_set: Name of the config set directory.

        Raises:
            CompositorError: If the config set doesn't exist or copy fails.
        """
        if not self._config_dir:
            raise CompositorError(
                "Cannot set config: no runtime config directory "
                "(compositor not started in headless mode)"
            )

        src_dir = os.path.join(self._config_base_dir, config_set)
        if not os.path.isdir(src_dir):
            raise CompositorError(f"Config set not found: {src_dir}")

        # Clear existing config files
        for f in os.listdir(self._config_dir):
            fp = os.path.join(self._config_dir, f)
            if os.path.isfile(fp):
                os.unlink(fp)

        # Copy all files from the config set
        for f in os.listdir(src_dir):
            src = os.path.join(src_dir, f)
            dst = os.path.join(self._config_dir, f)
            if os.path.isfile(src):
                shutil.copy2(src, dst)

    def wait_ready(self, timeout=5.0):
        """Wait for IPC socket to appear and respond to ping.

        Args:
            timeout: Maximum seconds to wait.

        Returns:
            True if the compositor is ready, False on timeout.
        """
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if self._socket_path and os.path.exists(self._socket_path):
                try:
                    resp = self._ipc_send({"command": "ping"})
                    if resp and resp.get("success"):
                        return True
                except (OSError, CompositorError, json.JSONDecodeError):
                    pass
            time.sleep(0.1)
        return False

    def is_running(self):
        """Check if compositor process is still alive.

        For live sessions, checks IPC connectivity instead.
        """
        if self._is_live_session:
            try:
                resp = self._ipc_send({"command": "ping"})
                return resp and resp.get("success") is True
            except (OSError, CompositorError):
                return False
        return self._process is not None and self._process.poll() is None

    @property
    def socket_path(self):
        """Return the IPC socket path."""
        return self._socket_path

    @property
    def runtime_dir(self):
        """Return the XDG_RUNTIME_DIR being used."""
        return self._runtime_dir

    @property
    def wayland_display(self):
        """Return the WAYLAND_DISPLAY name (e.g. 'wayland-0')."""
        return self._wayland_display

    @property
    def is_live_session(self):
        """Return True if connected to a live session (not headless)."""
        return self._is_live_session

    @property
    def process(self):
        """Return the compositor subprocess (None for live sessions)."""
        return self._process

    def _ipc_command(self, action):
        """Send a command action via IPC.

        Args:
            action: The action string (e.g. "Exit", "Reconfigure").

        Returns:
            Parsed JSON response dict.
        """
        return self._ipc_send({"command": "command", "action": action})

    def _ipc_send(self, message, timeout=3.0):
        """Send a JSON message to the IPC socket and read the response.

        Args:
            message: Dict to serialize as JSON.
            timeout: Socket timeout in seconds.

        Returns:
            Parsed JSON response dict.

        Raises:
            CompositorError: If socket communication fails.
        """
        if not self._socket_path:
            raise CompositorError("No IPC socket path configured")

        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        sock.settimeout(timeout)
        try:
            sock.connect(self._socket_path)
            # Send newline-delimited JSON
            data = json.dumps(message) + "\n"
            sock.sendall(data.encode("utf-8"))

            # Read response (newline-delimited)
            response_data = b""
            while b"\n" not in response_data:
                chunk = sock.recv(4096)
                if not chunk:
                    break
                response_data += chunk

            if not response_data:
                raise CompositorError("Empty response from IPC")

            # Parse up to first newline
            line = response_data.split(b"\n", 1)[0]
            return json.loads(line.decode("utf-8"))
        except socket.error as e:
            raise CompositorError(f"IPC socket error: {e}") from e
        finally:
            sock.close()

    def ipc_send(self, message, timeout=3.0):
        """Public interface to send a JSON message to the IPC socket.

        Args:
            message: Dict to serialize as JSON.
            timeout: Socket timeout in seconds.

        Returns:
            Parsed JSON response dict.
        """
        return self._ipc_send(message, timeout)

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.stop()
        return False
