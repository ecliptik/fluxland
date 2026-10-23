"""X11 (XWayland) windows are managed like Wayland windows.

Launches real X11 clients (xterm, needs XWayland) and checks that they
get decorations, focus, window commands, workspaces and the iconbar, and
that X11 knows where its windows are (via xwininfo), so X11 menus and
dialogs open in the right place.
"""

import os
import re
import shutil
import subprocess
import time

import pytest

SETTLE = 0.5

pytestmark = pytest.mark.skipif(
    not shutil.which("xterm") or not shutil.which("Xwayland"),
    reason="xterm and Xwayland are required")


def wait_for(ipc, title, timeout=8.0):
    """Wait for a mapped window with this title to appear."""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        win = ipc.get_window_by_title(title)
        if win is not None and win.get("width", 0) > 0:
            return win
        time.sleep(0.1)
    raise TimeoutError(f"X11 window '{title}' did not appear")


def focused_workspace(ipc):
    return next(w for w in ipc.get_workspaces() if w["focused"])


def wait_gone(ipc, title, timeout=5.0):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if ipc.get_window_by_title(title) is None:
            return True
        time.sleep(0.1)
    return False


def x_display_of(pid):
    """DISPLAY of an X11 client (the compositor's Xwayland)."""
    try:
        with open(f"/proc/{pid}/environ", "rb") as f:
            for var in f.read().split(b"\0"):
                if var.startswith(b"DISPLAY="):
                    return var[len(b"DISPLAY="):].decode()
    except OSError:
        pass
    return None


def x_geometry(display, title):
    """Absolute X11 position and size of a window, via xwininfo."""
    out = subprocess.run(
        ["xwininfo", "-name", title],
        env={**os.environ, "DISPLAY": display},
        capture_output=True, text=True, timeout=5).stdout

    def field(name):
        m = re.search(rf"{name}:\s+(-?\d+)", out)
        return int(m.group(1)) if m else None

    return (field("Absolute upper-left X"), field("Absolute upper-left Y"),
            field("Width"), field("Height"))


@pytest.fixture
def xterm(ipc):
    """An xterm with a unique title, closed afterwards."""
    title = f"XTest{os.getpid()}{time.monotonic_ns() % 100000}"
    ipc.command(f"Exec xterm -title {title}")
    win = wait_for(ipc, title)
    yield title, win
    current = ipc.get_window_by_title(title)
    if current is not None:
        try:
            ipc.command("Close", window=current["id"])
        except Exception:
            pass
        wait_gone(ipc, title)


def test_x11_window_is_managed(ipc, xterm):
    """An X11 window is listed, decorated and focused like any other."""
    title, win = xterm
    assert win["xwayland"] is True
    assert win["app_id"] == "XTerm"
    assert win["decorated"] is True
    assert win["focused"] is True
    assert win["workspace"] == focused_workspace(ipc)["index"]


def test_x11_close_by_window_id(ipc, xterm):
    """Close (WM_DELETE_WINDOW) reaches the X11 client."""
    title, win = xterm
    ipc.command("Close", window=win["id"])
    assert wait_gone(ipc, title)


def test_x11_maximize_and_restore(ipc, xterm):
    title, win = xterm
    output = ipc.get_outputs()[0]
    ipc.command("Maximize", window=win["id"])
    time.sleep(SETTLE)
    maxed = ipc.get_window_by_title(title)
    assert maxed["maximized"] is True
    assert maxed["width"] >= output["width"] * 0.8
    ipc.command("Maximize", window=win["id"])
    time.sleep(SETTLE)
    restored = ipc.get_window_by_title(title)
    assert restored["maximized"] is False
    assert abs(restored["width"] - win["width"]) <= 2


def test_x11_minimize_and_restore(ipc, xterm):
    title, win = xterm
    ipc.command("Minimize", window=win["id"])
    time.sleep(SETTLE)
    assert ipc.get_window_by_title(title)["minimized"] is True
    ipc.command("Deiconify")
    time.sleep(SETTLE)
    restored = ipc.get_window_by_title(title)
    assert restored["minimized"] is False
    assert restored["focused"] is True


def test_x11_send_to_workspace(ipc, xterm):
    title, win = xterm
    start = focused_workspace(ipc)["index"]
    target = (start + 1) % len(ipc.get_workspaces())
    ipc.command(f"SendToWorkspace {target + 1}", window=win["id"])
    time.sleep(SETTLE)
    assert ipc.get_window_by_title(title)["workspace"] == target
    ipc.command(f"SendToWorkspace {start + 1}", window=win["id"])
    time.sleep(SETTLE)


@pytest.mark.skipif(not shutil.which("xwininfo"),
                    reason="xwininfo (x11-utils) is required")
def test_x11_knows_its_position(ipc, xterm):
    """After a move/resize, X11 sees the client area where it is drawn."""
    title, win = xterm
    pid = win.get("pid")
    display = x_display_of(pid) if pid and pid > 0 else None
    if not display:
        pytest.skip("cannot find the Xwayland display")

    ipc.command("MoveTo 100 200", window=win["id"])
    ipc.command("ResizeTo 600 400", window=win["id"])
    time.sleep(SETTLE)
    frame = ipc.get_window_by_title(title)
    x, y, w, h = x_geometry(display, title)
    # Client area sits inside the frame (border + titlebar offset)
    assert frame["x"] <= x < frame["x"] + 20
    assert frame["y"] <= y < frame["y"] + 60
    assert (w, h) == (600, 400)


def test_x11_in_window_cycling(ipc, xterm, windows):
    """NextWindow walks Wayland and X11 windows alike."""
    title, win = xterm
    windows.open("WaylandPeer")
    time.sleep(SETTLE)
    assert ipc.get_focused_window()["title"] == "WaylandPeer"
    ipc.command("NextWindow (workspace=[current])")
    time.sleep(SETTLE)
    assert ipc.get_focused_window()["title"] == title
