"""Windows go back to a monitor that comes back.

An output that leaves the layout (a monitor powered off or unplugged, or turned
off by a profile) has its windows moved to the ones that remain; X kept the
screen as it was.  So that nothing is lost when a monitor is off for the night,
those windows go back to it when it returns, if they were left alone.

Two heads: HEADLESS-2 at 0,0 with the toolbar, HEADLESS-1 at 1280,0.
"""

import shutil
import subprocess
import time

import pytest

from lib.compositor import CompositorManager
from lib.ipc_client import FluxlandIPC

SETTLE = 0.7

pytestmark = pytest.mark.skipif(
    not (shutil.which("wlr-randr") and shutil.which("foot")),
    reason="needs wlr-randr and foot")

INIT = [
    "session.output.HEADLESS-1.mode: 1024x768",
    "session.output.HEADLESS-1.position: 1280,0",
    "session.output.HEADLESS-2.position: 0,0",
    "session.screen0.toolbar.onhead: 1",
]

LEFT, RIGHT = "HEADLESS-2", "HEADLESS-1"


@pytest.fixture(scope="module")
def comp():
    mgr = CompositorManager()
    mgr.start(config_set="default", headless=True, outputs=2,
              init_extra=INIT)
    yield mgr
    mgr.stop()


@pytest.fixture
def ipc(comp):
    client = FluxlandIPC(socket_path=comp.socket_path)
    yield client
    try:
        back(comp)
        client.command("CloseAllWindows")
        time.sleep(0.3)
    except Exception:
        pass
    client.close()


def randr(comp, *args):
    subprocess.run(["wlr-randr", *args], env=comp.client_env, check=True,
                   capture_output=True)
    time.sleep(SETTLE)


def back(comp):
    # as av-output-switcher puts it: where it was
    randr(comp, "--output", RIGHT, "--on", "--pos", "1280,0")


def output(ipc, name):
    return {o["name"]: o for o in ipc.get_outputs()}[name]


def open_window(ipc, app_id):
    ipc.command(f"Exec foot --app-id {app_id}")
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        win = ipc.get_window_by_app_id(app_id)
        if win and win["width"] > 0:
            return win
        time.sleep(0.1)
    raise TimeoutError(app_id)


def on_right(ipc, app_id, x=1400, y=200):
    win = open_window(ipc, app_id)
    ipc.command("SetHead 2", window=win["id"])
    time.sleep(SETTLE)
    ipc.command(f"MoveTo {x} {y}", window=win["id"])
    time.sleep(SETTLE)
    return ipc.get_window_by_app_id(app_id)


def corner(ipc, app_id):
    f = ipc.get_window_by_app_id(app_id)["frame"]
    return f["x"], f["y"]


def test_windows_go_back_when_the_monitor_does(comp, ipc):
    on_right(ipc, "Stays")
    win = on_right(ipc, "Goes", 300, 250)
    before = corner(ipc, "Goes")
    left = output(ipc, LEFT)["layout"]
    ipc.command("SetHead 1", window=ipc.get_window_by_app_id("Stays")["id"])
    time.sleep(SETTLE)
    own = corner(ipc, "Stays")

    randr(comp, "--output", RIGHT, "--off")
    x, y = corner(ipc, "Goes")
    # it was moved onto the output that remains
    assert left["x"] <= x < left["x"] + left["width"]

    back(comp)
    right = output(ipc, RIGHT)["layout"]
    x, y = corner(ipc, "Goes")
    assert right["x"] <= x < right["x"] + right["width"]
    # where it was on the monitor, as the monitor is placed now
    assert (x - right["x"], y - right["y"]) == (before[0] - 1280, before[1])
    # the window that was on the other monitor all along was left alone
    assert corner(ipc, "Stays") == own


def test_a_window_moved_meanwhile_stays_where_it_was_put(comp, ipc):
    win = on_right(ipc, "Moved", 300, 250)
    randr(comp, "--output", RIGHT, "--off")
    ipc.command("MoveTo 100 100", window=win["id"])
    time.sleep(SETTLE)
    back(comp)
    assert corner(ipc, "Moved") == (100, 100)


def test_a_maximized_window_is_maximized_there_again(comp, ipc):
    win = on_right(ipc, "Big")
    ipc.command("Maximize", window=win["id"])
    time.sleep(SETTLE)
    randr(comp, "--output", RIGHT, "--off")
    left = output(ipc, LEFT)["usable"]
    f = ipc.get_window_by_app_id("Big")["frame"]
    assert f["x"] == left["x"] and f["width"] <= left["width"] + 2

    back(comp)
    right = output(ipc, RIGHT)["usable"]
    big = ipc.get_window_by_app_id("Big")
    assert big["maximized"] is True
    assert big["frame"]["x"] == right["x"]
    assert abs(big["frame"]["width"] - right["width"]) <= 2
