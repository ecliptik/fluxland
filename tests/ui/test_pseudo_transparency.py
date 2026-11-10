"""Pseudo-transparency: a translucent window shows the desktop (the
wallpaper) behind it, not the windows below it.

The wallpaper is from swaybg, red on the left half and yellow on the right; an opaque green window sits under
a half transparent blue one.  With pseudo-transparency the blue window is
blended with the red wallpaper only; without, with the green window.
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
         and shutil.which("foot")),
    reason="needs swaybg, grim and foot")

INIT = [
    "session.screen0.toolbar.visible: false",
    "session.screen0.pseudoTransparency: false",
]


@pytest.fixture(scope="module")
def comp():
    mgr = CompositorManager()
    mgr.start(config_set="default", headless=True, outputs=1,
              init_extra=INIT)
    # Red on the left half, yellow on the right: where a window is shows
    wall = os.path.join(mgr.config_dir, "wall.png")
    img = Image.new("RGB", (1280, 720), (255, 0, 0))
    img.paste((255, 255, 0), (640, 0, 1280, 720))
    img.save(wall)
    bg = subprocess.Popen(["swaybg", "-m", "stretch", "-i", wall],
                          env=mgr.client_env,
                          stdout=subprocess.DEVNULL,
                          stderr=subprocess.DEVNULL)
    # a window is dragged by its titlebar
    with open(os.path.join(mgr.config_dir, "keys"), "w") as f:
        f.write("OnTitlebar Move1 :StartMoving\n")
    reload = FluxlandIPC(socket_path=mgr.socket_path)
    reload.command("Reconfigure")
    reload.close()
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
        client.command("SetResourceValue session.screen0.pseudoTransparency "
                       "false")
        client.command("CloseAllWindows")
        time.sleep(0.3)
    except Exception:
        pass
    client.close()


def open_window(ipc, app_id, bg, alpha):
    ipc.command(f"Exec foot --app-id {app_id} -o colors-dark.background={bg} "
                f"-o colors-dark.alpha={alpha}")
    deadline = time.monotonic() + 4
    while time.monotonic() < deadline:
        win = ipc.get_window_by_app_id(app_id)
        if win and win["width"] > 0:
            return win
        time.sleep(0.1)
    raise TimeoutError(app_id)


def pixel(comp, x, y):
    png = subprocess.run(["grim", "-g", f"{x},{y} 1x1", "-t", "png", "-"],
                         check=True, capture_output=True,
                         env=comp.client_env).stdout
    return Image.open(io.BytesIO(png)).convert("RGB").getpixel((0, 0))


def setup_windows(ipc):
    under = open_window(ipc, "Under", "00ff00", 1.0)
    ipc.command("MoveTo 100 100", window=under["id"])
    ipc.command("ResizeTo 600 400", window=under["id"])
    over = open_window(ipc, "Over", "0000ff", 0.5)
    ipc.command("MoveTo 150 150", window=over["id"])
    ipc.command("ResizeTo 400 250", window=over["id"])
    time.sleep(SETTLE)
    f = ipc.get_window_by_app_id("Over")["frame"]
    # Inside the content, clear of the titlebar and the borders
    return f["x"] + f["width"] // 2, f["y"] + f["height"] - 30


def test_without_it_the_window_below_shows_through(comp, ipc):
    x, y = setup_windows(ipc)
    r, g, b = pixel(comp, x, y)
    # blue over green: no red from the wallpaper
    assert g > 60 and r < 40 and b > 60


def test_with_it_only_the_wallpaper_does(comp, ipc):
    x, y = setup_windows(ipc)
    ipc.command("SetResourceValue session.screen0.pseudoTransparency true")
    time.sleep(SETTLE)
    r, g, b = pixel(comp, x, y)
    # blue over red: red and blue, no green from the window below
    assert r > 60 and b > 60 and g < 40


def test_a_window_rule_beats_the_global_setting(comp, ipc):
    x, y = setup_windows(ipc)
    ipc.command("SetResourceValue session.screen0.pseudoTransparency true")
    time.sleep(SETTLE)
    over = ipc.get_window_by_app_id("Over")
    ipc.command("PseudoTransparent off", window=over["id"])
    time.sleep(SETTLE)
    r, g, b = pixel(comp, x, y)
    assert g > 60 and r < 40


def test_it_follows_the_window(comp, ipc):
    x, y = setup_windows(ipc)
    ipc.command("SetResourceValue session.screen0.pseudoTransparency true")
    time.sleep(SETTLE)
    assert pixel(comp, x, y)[0] > 100 and pixel(comp, x, y)[1] < 40

    # Both windows over to the yellow half: blue over yellow, and the green
    # window below still not seen
    for app_id, (mx, my) in (("Under", (700, 100)), ("Over", (750, 150))):
        win = ipc.get_window_by_app_id(app_id)
        ipc.command(f"MoveTo {mx} {my}", window=win["id"])
    time.sleep(SETTLE)
    f = ipc.get_window_by_app_id("Over")["frame"]
    r, g, b = pixel(comp, f["x"] + f["width"] // 2,
                    f["y"] + f["height"] - 30)
    assert r > 60 and g > 100 and b > 60


@pytest.mark.skipif(not _find_test_pointer(),
                    reason="fluxland-test-pointer is not built")
def test_it_follows_a_window_dragged_by_hand(comp, ipc):
    """The pointer code moves a window without going through the usual
    move; the copy of the desktop under it still follows, as it moves."""
    ipc.command("SetResourceValue session.screen0.pseudoTransparency true")
    win = open_window(ipc, "Dragged", "0000ff", 0.5)
    ipc.command("MoveTo 100 100", window=win["id"])
    ipc.command("ResizeTo 300 200", window=win["id"])
    time.sleep(SETTLE)
    f = ipc.get_window_by_app_id("Dragged")["frame"]
    start = (f["x"] + f["width"] // 2, f["y"] + 14)
    # to the yellow half, and held there while the screen is looked at
    end = (start[0] + 1000, start[1])
    drag = subprocess.Popen(
        [_find_test_pointer(), "move", str(start[0]), str(start[1]),
         "press", "1", "sleep", "100", "to", str(start[0] + 300),
         str(start[1]), "sleep", "100", "to", str(end[0]), str(end[1]),
         "sleep", "3000", "release", "1"],
        env=comp.client_env)
    try:
        time.sleep(1.5)
        moved = ipc.get_window_by_app_id("Dragged")["frame"]
        assert moved["x"] > 600, "the window was dragged to the right half"
        r, g, b = pixel(comp, moved["x"] + moved["width"] // 2,
                        moved["y"] + moved["height"] // 2)
        # blue over yellow, not over the red it was dragged from
        assert r > 60 and g > 100 and b > 60, (r, g, b)
    finally:
        drag.wait(timeout=10)
