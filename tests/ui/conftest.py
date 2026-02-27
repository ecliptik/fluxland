"""Pytest fixtures for fluxland UI tests.

Provides session-scoped compositor management and per-test IPC, screenshot,
and input driver fixtures. Supports both live session testing (default) and
headless mode (set FLUXLAND_TEST_HEADLESS=1).
"""

import os
import subprocess
import time

import pytest

from lib.compositor import CompositorManager
from lib.ipc_client import FluxlandIPC
from lib.input_driver import InputDriver
from lib.report import ReportGenerator, TestReport, TestResult
from lib.screenshot import ScreenshotEngine


# ---------------------------------------------------------------------------
# Command-line options
# ---------------------------------------------------------------------------

def pytest_addoption(parser):
    parser.addoption(
        "--bootstrap-screenshots",
        action="store_true",
        default=False,
        help="Save captured screenshots as new references instead of comparing.",
    )
    parser.addoption(
        "--update-reference",
        action="store_true",
        default=False,
        help="Update reference images for tests that run.",
    )


# ---------------------------------------------------------------------------
# Session-scoped: compositor lifecycle
# ---------------------------------------------------------------------------

@pytest.fixture(scope="session")
def compositor():
    """Session-scoped compositor instance.

    In live mode (default): connects to the running fluxland session.
    In headless mode (FLUXLAND_TEST_HEADLESS=1): starts a headless compositor.
    """
    headless = os.environ.get("FLUXLAND_TEST_HEADLESS", "0") == "1"

    # Auto-detect: if no WAYLAND_DISPLAY is set, must use headless mode
    if not headless and not os.environ.get("WAYLAND_DISPLAY"):
        headless = True

    mgr = CompositorManager()
    mgr.start(config_set="default", headless=headless)
    yield mgr
    mgr.stop()


# ---------------------------------------------------------------------------
# Per-test fixtures
# ---------------------------------------------------------------------------

@pytest.fixture
def ipc(compositor):
    """Per-test IPC client connected to the running compositor."""
    client = FluxlandIPC(socket_path=compositor.socket_path)
    yield client
    client.close()


@pytest.fixture
def screenshot(compositor, request):
    """Per-test screenshot engine.

    Reference images live in tests/ui/reference/.
    Results go to tests/ui/results/.
    """
    ui_dir = os.path.dirname(__file__)
    engine = ScreenshotEngine(
        reference_dir=os.path.join(ui_dir, "reference"),
        results_dir=os.path.join(ui_dir, "results"),
    )

    # If --bootstrap-screenshots, we could set a flag here
    # (the engine already bootstraps when no reference exists)
    return engine


@pytest.fixture
def input_drv():
    """Per-test input driver for keyboard/pointer simulation."""
    return InputDriver()


# ---------------------------------------------------------------------------
# Test window management
# ---------------------------------------------------------------------------

class WindowHelper:
    """Helper for creating and managing test windows via IPC.

    Launches foot terminals as test surfaces. Tracks PIDs and titles
    for cleanup. Waits for windows to appear in the compositor before
    returning.
    """

    def __init__(self, ipc):
        self._ipc = ipc
        self._titles = []

    def open(self, title="Test Window", wait=True, timeout=2.0):
        """Open a test window with the given title.

        Args:
            title: Window title (passed to foot --title).
            wait: If True, block until the window appears in get_windows.
            timeout: Max seconds to wait for the window to appear.

        Returns:
            The window dict from get_windows if wait=True, else None.
        """
        self._ipc.command(f"Exec foot --title {title}")
        self._titles.append(title)

        if not wait:
            return None

        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            win = self._ipc.get_window_by_title(title)
            if win is not None:
                return win
            time.sleep(0.1)

        raise TimeoutError(
            f"Window '{title}' did not appear within {timeout}s"
        )

    def open_many(self, count, prefix="Win", wait=True):
        """Open multiple test windows.

        Args:
            count: Number of windows to create.
            prefix: Title prefix (windows get titles like "Win1", "Win2"...).
            wait: If True, wait for all windows to appear.

        Returns:
            List of window dicts if wait=True.
        """
        windows = []
        for i in range(1, count + 1):
            title = f"{prefix}{i}"
            win = self.open(title=title, wait=wait)
            if win:
                windows.append(win)
        return windows

    def close_all(self):
        """Close all windows created by this helper."""
        try:
            self._ipc.command("CloseAllWindows")
        except Exception:
            pass
        time.sleep(0.3)
        self._titles.clear()

    def wait_for_window_count(self, expected, timeout=3.0):
        """Wait until the compositor reports exactly N windows.

        Returns:
            The list of windows when the count matches.

        Raises:
            TimeoutError if the count doesn't match within timeout.
        """
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            windows = self._ipc.get_windows()
            if len(windows) == expected:
                return windows
            time.sleep(0.1)
        actual = len(self._ipc.get_windows())
        raise TimeoutError(
            f"Expected {expected} windows, got {actual} after {timeout}s"
        )


@pytest.fixture
def windows(ipc):
    """Per-test window helper with automatic cleanup.

    Usage:
        def test_example(ipc, windows):
            windows.open("MyWindow")
            win = ipc.get_window_by_title("MyWindow")
            assert win is not None
    """
    helper = WindowHelper(ipc)
    yield helper
    helper.close_all()


# ---------------------------------------------------------------------------
# Auto-cleanup
# ---------------------------------------------------------------------------

@pytest.fixture(autouse=True)
def clean_state(compositor):
    """Reset compositor state after each test.

    Hides menus, closes all test windows, and switches to workspace 0.
    Runs after the test (yield-based teardown).
    """
    yield

    # Post-test cleanup via IPC
    try:
        client = FluxlandIPC(socket_path=compositor.socket_path, timeout=2.0)
        try:
            client.command("HideMenus")
        except Exception:
            pass
        # Unstick any sticky windows first
        try:
            for win in client.get_windows():
                if win.get("sticky"):
                    client.command("Stick")
        except Exception:
            pass
        # Close all windows on the current workspace
        try:
            if client.get_windows():
                client.command("CloseAllWindows")
                time.sleep(0.3)
        except Exception:
            pass

        # If windows remain on other workspaces, collapse all workspaces
        # to 1 (RemoveLastWorkspace moves views to previous workspace),
        # then close again, then restore workspace count.
        try:
            remaining = client.get_windows()
            if remaining:
                ws_list = client.get_workspaces()
                ws_count = len(ws_list)
                if ws_count > 1:
                    for _ in range(ws_count - 1):
                        client.command("RemoveLastWorkspace")
                        time.sleep(0.1)
                    # All windows now on workspace 0
                    client.command("CloseAllWindows")
                    time.sleep(0.3)
                    # Restore original workspace count
                    for _ in range(ws_count - 1):
                        client.command("AddWorkspace")
                        time.sleep(0.1)
        except Exception:
            pass

        try:
            client.command("Workspace 0")
        except Exception:
            pass

        # Wait for windows to fully close
        deadline = time.monotonic() + 2.0
        while time.monotonic() < deadline:
            try:
                if not client.get_windows():
                    break
            except Exception:
                break
            time.sleep(0.1)
        client.close()
    except Exception:
        pass


# ---------------------------------------------------------------------------
# Report generation hooks
# ---------------------------------------------------------------------------

_test_report = TestReport()


def pytest_sessionstart(session):
    """Record session start time."""
    _test_report.start_time = time.time()


def pytest_runtest_logreport(report):
    """Collect test results (only the call phase, or setup/teardown errors)."""
    if report.when == "call":
        msg = ""
        if report.failed and report.longreprtext:
            msg = report.longreprtext
        elif report.skipped and hasattr(report, "wasxfail"):
            msg = report.wasxfail
        elif report.skipped:
            msg = str(report.longrepr[2]) if report.longrepr else ""
        _test_report.results.append(
            TestResult(
                nodeid=report.nodeid,
                outcome=report.outcome,
                duration=report.duration,
                message=msg,
            )
        )
    elif report.when in ("setup", "teardown") and report.failed:
        _test_report.results.append(
            TestResult(
                nodeid=report.nodeid,
                outcome="failed",
                duration=report.duration,
                message=f"[{report.when}] {report.longreprtext}",
            )
        )


def pytest_sessionfinish(session, exitstatus):
    """Generate HTML and JSON reports at end of test session."""
    _test_report.end_time = time.time()
    if not _test_report.results:
        return

    results_dir = os.path.join(os.path.dirname(__file__), "results")
    gen = ReportGenerator(results_dir)
    try:
        json_path = gen.write_json(_test_report)
        html_path = gen.write_html(_test_report)
        print(f"\nReport: {html_path}")
    except Exception as e:
        print(f"\nWarning: failed to generate report: {e}")
