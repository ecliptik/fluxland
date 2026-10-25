"""Fluxbox keys-file behaviour in a running compositor.

Runs its own headless compositor with a keys file of Fluxbox bindings:
"Double Mouse1" on the titlebar, and commands with Fluxbox's arguments
(MoveTo with % and corners, ResizeTo, Move by deltas, workspace offsets)
sent over IPC. The pointer is driven with wlrctl, which can't make wheel
steps: test_wheel_bindings.c covers the wheel.
"""

import os
import shutil
import time

import pytest

from lib.compositor import CompositorManager
from lib.input_driver import InputDriver
from lib.ipc_client import FluxlandIPC

SETTLE = 0.4

KEYS = """\
OnDesktop Mouse4 :NextWorkspace
OnDesktop Mouse5 :PrevWorkspace
OnTitlebar Double Mouse1 :Shade
OnTitlebar Move1 :StartMoving
OnWindow Mod1 Mouse1 :MacroCmd {Raise} {Focus} {StartMoving}
"""

needs_wlrctl = pytest.mark.skipif(not shutil.which("wlrctl"),
                                  reason="wlrctl is not installed")


@pytest.fixture(scope="module")
def comp():
    mgr = CompositorManager()
    mgr.start(config_set="default", headless=True)
    with open(os.path.join(mgr.config_dir, "keys"), "w") as f:
        f.write(KEYS)
    client = FluxlandIPC(socket_path=mgr.socket_path)
    client.command("Reconfigure")
    client.close()
    time.sleep(SETTLE)
    yield mgr
    mgr.stop()


@pytest.fixture
def kipc(comp):
    client = FluxlandIPC(socket_path=comp.socket_path)
    client.command("Workspace 1")
    yield client
    try:
        client.command("CloseAllWindows")
        deadline = time.monotonic() + 4
        while client.get_windows() and time.monotonic() < deadline:
            time.sleep(0.1)
        client.command("Workspace 1")
    except Exception:
        pass
    client.close()


@pytest.fixture
def pointer(comp):
    return InputDriver(env=comp.client_env)


def current_workspace(ipc):
    for ws in ipc.get_workspaces():
        if ws.get("focused") or ws.get("active") or ws.get("current"):
            return ws["index"] if "index" in ws else ws.get("id")
    raise AssertionError("no current workspace")


def open_window(ipc, title):
    ipc.command(f"Exec foot --title {title}")
    deadline = time.monotonic() + 4
    while time.monotonic() < deadline:
        win = ipc.get_window_by_title(title)
        if win and win["width"] > 0:
            time.sleep(0.2)
            return ipc.get_window_by_title(title)
        time.sleep(0.1)
    raise TimeoutError(title)


def usable(ipc):
    return ipc.get_outputs()[0]["usable"]


@needs_wlrctl
def test_double_click_titlebar_shades(kipc, pointer):
    win = open_window(kipc, "KeysShade")
    # The middle of the titlebar, above the window's content
    pointer.double_click(win["x"] + win["width"] // 2, win["y"] + 6)
    time.sleep(SETTLE)
    assert kipc.get_window_by_title("KeysShade")["shaded"] is True


def test_move_to_percent_and_corner(kipc):
    win = open_window(kipc, "KeysMoveTo")
    area = usable(kipc)
    kipc.command("MoveTo 50% 0", window=win["id"])
    time.sleep(SETTLE)
    win = kipc.get_window_by_title("KeysMoveTo")
    assert win["x"] == area["x"] + area["width"] // 2
    assert win["y"] == area["y"]
    # "*" keeps x; from the bottom left corner
    kipc.command("MoveTo * 0 BottomLeft", window=win["id"])
    time.sleep(SETTLE)
    moved = kipc.get_window_by_title("KeysMoveTo")
    assert moved["x"] == win["x"]
    # The frame's bottom is on the head's bottom: y is the frame's top,
    # height the content's, the titlebar and handle the rest
    bottom = moved["y"] + moved["height"]
    assert area["y"] + area["height"] - 40 <= bottom
    assert bottom < area["y"] + area["height"]


def test_resize_to_percent(kipc):
    win = open_window(kipc, "KeysResizeTo")
    area = usable(kipc)
    kipc.command("ResizeTo 50% 50%", window=win["id"])
    time.sleep(SETTLE)
    win = kipc.get_window_by_title("KeysResizeTo")
    # The window, decorations included, is half the head (foot sizes in
    # character cells)
    assert area["width"] / 2 - 40 < win["width"] <= area["width"] / 2
    assert area["height"] / 2 - 60 < win["height"] <= area["height"] / 2


def test_move_by_delta(kipc):
    win = open_window(kipc, "KeysMove")
    kipc.command("Move 30 -10", window=win["id"])
    time.sleep(SETTLE)
    moved = kipc.get_window_by_title("KeysMove")
    assert moved["x"] == win["x"] + 30
    assert moved["y"] == win["y"] - 10


def test_workspace_offsets(kipc):
    start = current_workspace(kipc)
    kipc.command("NextWorkspace 2")
    time.sleep(SETTLE)
    assert current_workspace(kipc) == start + 2
    # 0: back to the workspace before
    kipc.command("NextWorkspace 0")
    time.sleep(SETTLE)
    assert current_workspace(kipc) == start
    # -1 is the last one
    kipc.command("Workspace -1")
    time.sleep(SETTLE)
    assert current_workspace(kipc) == len(kipc.get_workspaces()) - 1
