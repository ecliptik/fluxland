"""Heads are numbered by position, and the toolbar can be given a monitor by
name.

Head numbers are left to right (then top to bottom) whatever order the
monitors connected in, so a monitor unplugged and plugged again is the same
head.  The toolbar (and slit) go on session.screen0.toolbar.onoutput, a
connector name, while that monitor is there, else on toolbar.onhead.

Two heads: HEADLESS-1 at 0,0 (the left one) and HEADLESS-2 at 1280,0.  The
headless backend announces HEADLESS-2 first, so by connection order it would
be head 1.
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

LEFT, RIGHT = "HEADLESS-1", "HEADLESS-2"

INIT = [
    f"session.output.{LEFT}.mode: 1024x768",
    f"session.output.{LEFT}.position: 0,0",
    f"session.output.{RIGHT}.position: 1280,0",
    "session.screen0.toolbar.onhead: 2",
    f"session.screen0.toolbar.onoutput: {LEFT}",
]


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
        client.command("SetToolbarOutput")
        put_back(comp, LEFT, "0,0")
        put_back(comp, RIGHT, "1280,0")
        client.command("CloseAllWindows")
        time.sleep(0.3)
    except Exception:
        pass
    client.close()


def randr(comp, *args):
    subprocess.run(["wlr-randr", *args], env=comp.client_env, check=True,
                   capture_output=True)
    time.sleep(SETTLE)


def put_back(comp, name, pos):
    randr(comp, "--output", name, "--on", "--pos", pos)


def toolbar_on(ipc):
    """The output the toolbar reserves space on."""
    reserved = [o["name"] for o in ipc.get_outputs()
                if o["enabled"] and o["usable"]["height"] < o["layout"]["height"]]
    assert len(reserved) == 1, reserved
    return reserved[0]


def open_window(ipc, app_id):
    ipc.command(f"Exec foot --app-id {app_id}")
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        win = ipc.get_window_by_app_id(app_id)
        if win and win["width"] > 0:
            return win
        time.sleep(0.1)
    raise TimeoutError(app_id)


def test_head_one_is_the_leftmost_not_the_first_connected(ipc):
    win = open_window(ipc, "HeadOne")
    ipc.command("SetHead 1", window=win["id"])
    time.sleep(SETTLE)
    assert ipc.get_window_by_app_id("HeadOne")["frame"]["x"] < 1024
    ipc.command("SetHead 2", window=win["id"])
    time.sleep(SETTLE)
    assert ipc.get_window_by_app_id("HeadOne")["frame"]["x"] >= 1280


def test_the_numbers_do_not_change_when_a_monitor_is_replugged(comp, ipc):
    win = open_window(ipc, "Replug")
    # the left one off and on again: it connects last, by connection order
    randr(comp, "--output", LEFT, "--off")
    put_back(comp, LEFT, "0,0")
    ipc.command("SetHead 1", window=win["id"])
    time.sleep(SETTLE)
    assert ipc.get_window_by_app_id("Replug")["frame"]["x"] < 1024
    ipc.command("SetHead 2", window=win["id"])
    time.sleep(SETTLE)
    assert ipc.get_window_by_app_id("Replug")["frame"]["x"] >= 1280


def test_the_toolbar_is_on_the_output_it_is_named_for(ipc):
    # toolbar.onoutput (the left one) beats toolbar.onhead (the second: the
    # right one)
    assert toolbar_on(ipc) == LEFT


def test_set_toolbar_output_and_head(ipc):
    ipc.command(f"SetToolbarOutput {RIGHT}")
    time.sleep(SETTLE)
    assert toolbar_on(ipc) == RIGHT
    ipc.command("SetToolbarHead 1")
    time.sleep(SETTLE)
    assert toolbar_on(ipc) == LEFT
    # nothing: the configured place again
    ipc.command(f"SetToolbarOutput {RIGHT}")
    time.sleep(SETTLE)
    ipc.command("SetToolbarOutput")
    time.sleep(SETTLE)
    assert toolbar_on(ipc) == LEFT


def test_the_toolbar_goes_back_to_a_monitor_that_returns(comp, ipc):
    randr(comp, "--output", LEFT, "--off")
    assert toolbar_on(ipc) == RIGHT		# the only one there
    put_back(comp, LEFT, "0,0")
    assert toolbar_on(ipc) == LEFT		# toolbar.onoutput again


def test_a_chosen_output_is_come_back_to_too(comp, ipc):
    ipc.command(f"SetToolbarOutput {RIGHT}")
    time.sleep(SETTLE)
    randr(comp, "--output", RIGHT, "--off")
    assert toolbar_on(ipc) == LEFT
    put_back(comp, RIGHT, "1280,0")
    assert toolbar_on(ipc) == RIGHT
