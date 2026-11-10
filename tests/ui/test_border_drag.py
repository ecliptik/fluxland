"""A window can be dragged by the top of its border, even the topmost pixel
of the screen.

The border is a rectangle of the frame, not a buffer, and the pointer lookup
passed over rectangles: a window at the top of the screen could not be taken
by the topmost pixel, which is its border.  The border above the titlebar is
the titlebar's (OnTitlebar Move1 :StartMoving moves by it); the other sides are
OnWindowBorder, which has no binding here: nothing happens by them (Fluxbox's
default keys bind it to the same move, and session.ignoreBorder turns that
off).
"""

import os
import subprocess
import time

import pytest

from lib.compositor import CompositorManager
from lib.input_driver import _find_test_pointer
from lib.ipc_client import FluxlandIPC

SETTLE = 0.5

pytestmark = pytest.mark.skipif(
    not _find_test_pointer(), reason="fluxland-test-pointer is not built")


@pytest.fixture(scope="module")
def comp():
    mgr = CompositorManager()
    mgr.start(config_set="default", headless=True, outputs=1,
              init_extra=["session.screen0.toolbar.visible: false",
                          "session.screen0.workspacewarping: false"])
    with open(os.path.join(mgr.config_dir, "keys"), "w") as f:
        f.write("OnTitlebar Move1 :StartMoving\n")
    client = FluxlandIPC(socket_path=mgr.socket_path)
    client.command("Reconfigure")
    client.close()
    time.sleep(SETTLE)
    yield mgr
    mgr.stop()


@pytest.fixture
def ipc(comp):
    client = FluxlandIPC(socket_path=comp.socket_path)
    yield client
    try:
        client.command("CloseAllWindows")
        time.sleep(0.3)
    except Exception:
        pass
    client.close()


def open_window(ipc, app_id):
    ipc.command(f"Exec foot --app-id {app_id}")
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        win = ipc.get_window_by_app_id(app_id)
        if win and win["width"] > 0:
            return win
        time.sleep(0.1)
    raise TimeoutError(app_id)


def drag(comp, x, y, dx, dy):
    subprocess.run(
        [_find_test_pointer(), "move", str(x), str(y), "press", "1",
         "sleep", "80", "to", str(x + dx // 3), str(y + dy // 3),
         "sleep", "80", "to", str(x + dx), str(y + dy), "sleep", "80",
         "release", "1"], env=comp.client_env, check=True)
    time.sleep(SETTLE)


def frame(ipc, app_id):
    return ipc.get_window_by_app_id(app_id)["frame"]


@pytest.mark.parametrize("name,point,moves", [
    ("top row", lambda f: (f["x"] + f["width"] // 2, f["y"]), True),
    ("titlebar", lambda f: (f["x"] + f["width"] // 2, f["y"] + 10), True),
    # no OnWindowBorder binding: the sides do nothing
    ("left column", lambda f: (f["x"], f["y"] + f["height"] // 2), False),
    ("right column", lambda f: (f["x"] + f["width"] - 1,
                                f["y"] + f["height"] // 2), False),
])
def test_dragging_by_the_top_border_moves_the_window(comp, ipc, name, point,
                                                    moves):
    win = open_window(ipc, "Border")
    ipc.command("MoveTo 300 0", window=win["id"])
    time.sleep(SETTLE)
    before = frame(ipc, "Border")
    x, y = point(before)
    drag(comp, x, y, 150, 120)
    after = frame(ipc, "Border")
    assert ((after["x"], after["y"]) != (before["x"], before["y"])) == moves, name
    assert after["width"] == before["width"]	# moved or not, not resized


def test_the_topmost_pixel_of_a_maximized_window_takes_it(comp, ipc):
    win = open_window(ipc, "TopMax")
    ipc.command("Maximize", window=win["id"])
    time.sleep(SETTLE)
    f = frame(ipc, "TopMax")
    assert f["y"] == 0
    # pulled down from the top row: off its maximize, as by the titlebar
    drag(comp, f["x"] + f["width"] // 2, 0, 20, 300)
    assert ipc.get_window_by_app_id("TopMax")["maximized"] is False
