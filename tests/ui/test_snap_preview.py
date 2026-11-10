"""The rectangle that shows where a dragged window would dock.

It is above every window but the one being dragged, which stays over it (as on
Windows), so the window is never tinted or hidden by the preview of where it
is going.
"""

import io
import os
import shutil
import subprocess
import time

import pytest
from PIL import Image

from lib.compositor import CompositorManager
from lib.input_driver import _find_test_pointer
from lib.ipc_client import FluxlandIPC

SETTLE = 0.6

pytestmark = pytest.mark.skipif(
    not (shutil.which("swaybg") and shutil.which("grim")
         and shutil.which("foot") and _find_test_pointer()),
    reason="needs swaybg, grim, foot and fluxland-test-pointer")

INIT = [
    "session.screen0.toolbar.visible: false",
    "session.screen0.enableWindowSnapping: true",
    "session.screen0.edgeSnapThreshold: 0",
    "session.screen0.workspacewarping: false",
]


@pytest.fixture(scope="module")
def comp():
    mgr = CompositorManager()
    mgr.start(config_set="default", headless=True, outputs=1,
              init_extra=INIT)
    with open(os.path.join(mgr.config_dir, "keys"), "w") as f:
        f.write("OnTitlebar Move1 :StartMoving\n")
    reload = FluxlandIPC(socket_path=mgr.socket_path)
    reload.command("Reconfigure")
    reload.close()
    bg = subprocess.Popen(["swaybg", "-c", "#ff0000"], env=mgr.client_env,
                          stdout=subprocess.DEVNULL,
                          stderr=subprocess.DEVNULL)
    time.sleep(1.0)
    yield mgr
    bg.terminate()
    bg.wait(timeout=5)
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


def pixel(comp, x, y):
    png = subprocess.run(["grim", "-g", f"{x},{y} 1x1", "-t", "png", "-"],
                         check=True, capture_output=True,
                         env=comp.client_env).stdout
    return Image.open(io.BytesIO(png)).convert("RGB").getpixel((0, 0))


def close(a, b, tol=14):
    return all(abs(p - q) <= tol for p, q in zip(a, b))


def test_the_dragged_window_stays_over_the_preview(comp, ipc):
    # an opaque green window, dragged to the left edge, held there
    ipc.command("Exec foot --app-id Dragged "
                "-o colors-dark.background=00ff00 -o colors-dark.alpha=1.0")
    deadline = time.monotonic() + 5
    win = None
    while time.monotonic() < deadline:
        win = ipc.get_window_by_app_id("Dragged")
        if win and win["width"] > 0:
            break
        time.sleep(0.1)
    ipc.command("MoveTo 300 200", window=win["id"])
    ipc.command("ResizeTo 300 200", window=win["id"])
    time.sleep(SETTLE)
    f = ipc.get_window_by_app_id("Dragged")["frame"]
    start = (f["x"] + f["width"] // 2, f["y"] + 14)

    drag = subprocess.Popen(
        [_find_test_pointer(), "move", str(start[0]), str(start[1]),
         "press", "1", "sleep", "100", "to", str(start[0] - 150),
         str(start[1] + 100), "sleep", "100", "to", "2", "400",
         "sleep", "3000", "release", "1"],
        env=comp.client_env)
    try:
        time.sleep(1.5)
        moved = ipc.get_window_by_app_id("Dragged")["frame"]
        inside = (moved["x"] + 40, moved["y"] + moved["height"] // 2)
        assert 0 <= inside[0] < 300 and moved["width"] > 100
        # where the window is, it is itself: not tinted by the preview
        assert close(pixel(comp, *inside), (0, 255, 0)), pixel(comp, *inside)
        # in the preview but not under the window, the wallpaper is tinted
        tinted = pixel(comp, 500, 650)
        assert tinted != (255, 0, 0) and close(tinted, (202, 38, 61), 30), tinted
        # and outside both, plain
        assert close(pixel(comp, 1000, 650), (255, 0, 0))
    finally:
        drag.wait(timeout=10)
