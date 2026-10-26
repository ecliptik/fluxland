"""The apps file, Fluxbox style, applied to real windows.

Each test writes the compositor's apps file and opens foot windows. Like
Fluxbox, fluxland re-reads a changed apps file when a window opens, so
no reconfigure is needed. Rules match on the window title (foot --title)
and use [Deco] {NONE}, so a window's size is its frame size.
"""

import os
import math
import time

import pytest

from lib.compositor import CompositorManager
from lib.ipc_client import FluxlandIPC

_mtime = [time.time() - 1000]


@pytest.fixture(scope="module")
def comp():
    mgr = CompositorManager()
    mgr.start(config_set="default", headless=True)
    yield mgr
    mgr.stop()


@pytest.fixture
def aipc(comp):
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


def set_apps(comp, text):
    path = os.path.join(comp.config_dir, "apps")
    with open(path, "w") as f:
        f.write(text)
    # A new mtime every time, even within one second
    _mtime[0] += 10
    os.utime(path, (_mtime[0], _mtime[0]))


def open_window(ipc, title, timeout=4.0):
    ipc.command(f"Exec foot --title {title}")
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        win = ipc.get_window_by_title(title)
        if win and win["width"] > 0:
            time.sleep(0.3)  # let the rule's configure land
            return ipc.get_window_by_title(title)
        time.sleep(0.1)
    raise TimeoutError(f"window {title} did not open")


def usable(ipc):
    return ipc.get_outputs()[0]["usable"]


def test_percent_dimensions_and_bottom_right(comp, aipc):
    set_apps(comp, "[app] (title=AppsCorner)\n"
                   "  [Deco] {NONE}\n"
                   "  [Dimensions] {50% 50%}\n"
                   "  [Position] (BottomRight) {0 0}\n"
                   "[end]\n")
    win = open_window(aipc, "AppsCorner")
    area = usable(aipc)
    # foot sizes in character cells: within one cell of the request
    assert abs(win["width"] - area["width"] / 2) < 24
    assert abs(win["height"] - area["height"] / 2) < 40
    # Anchored to the corner using the size asked for, 50% rounded as
    # Fluxbox rounds it (RelCalcHelper: half up, not Python's to even)
    half = lambda n: math.floor(n / 2 + 0.5)
    assert win["x"] == area["x"] + area["width"] - half(area["width"])
    assert win["y"] == area["y"] + area["height"] - half(area["height"])


def test_layer_number_and_workspace(comp, aipc):
    set_apps(comp, "[app] (title=AppsLayer)\n"
                   "  [Layer] {10}\n"
                   "  [Workspace] {2}\n"
                   "[end]\n")
    win = open_window(aipc, "AppsLayer")
    assert win["layer"] == "Below"
    assert win["workspace"] == 2


def test_match_limit(comp, aipc):
    set_apps(comp, "[app] (title=AppsLimit.*) {1}\n"
                   "  [Workspace] {1}\n"
                   "[end]\n")
    first = open_window(aipc, "AppsLimitA")
    second = open_window(aipc, "AppsLimitB")
    assert first["workspace"] == 1
    assert second["workspace"] == 0


def test_maximized_horz(comp, aipc):
    set_apps(comp, "[app] (title=AppsHorz)\n"
                   "  [Deco] {NONE}\n"
                   "  [Maximized] {horz}\n"
                   "[end]\n")
    win = open_window(aipc, "AppsHorz")
    area = usable(aipc)
    assert not win["maximized"]
    assert win["x"] == area["x"]
    assert abs(win["width"] - area["width"]) < 24


def test_focus_new_window_no(comp, aipc):
    set_apps(comp, "[app] (title=AppsQuiet)\n"
                   "  [FocusNewWindow] {no}\n"
                   "[end]\n")
    loud = open_window(aipc, "AppsLoud")
    assert loud["focused"]
    quiet = open_window(aipc, "AppsQuiet")
    assert not quiet["focused"]
    assert aipc.get_window_by_title("AppsLoud")["focused"]


def test_hidden_from_cycling(comp, aipc):
    set_apps(comp, "[app] (title=AppsHidden)\n"
                   "  [Hidden] {yes}\n"
                   "[end]\n")
    open_window(aipc, "AppsVisibleA")
    open_window(aipc, "AppsVisibleB")
    hidden = open_window(aipc, "AppsHidden")
    assert hidden["focused"]
    # NextWindow never comes back to it
    for _ in range(4):
        aipc.command("NextWindow")
        time.sleep(0.2)
        assert not aipc.get_window_by_title("AppsHidden")["focused"]
