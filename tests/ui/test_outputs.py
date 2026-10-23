"""Several monitors: output settings, heads, usable areas and hotplug.

Runs its own headless compositor with two outputs. The headless backend
announces HEADLESS-2 first, so it is head 1. Turning an output off and on
uses wlr-randr (wlr-output-management), when it is installed.
"""

import os
import shutil
import subprocess
import time

import pytest

from lib.compositor import CompositorManager
from lib.ipc_client import FluxlandIPC

SETTLE = 0.5

INIT = [
    "session.output.HEADLESS-1.mode: 1024x768",
    "session.output.HEADLESS-1.position: 1280,0",
    "session.output.HEADLESS-2.position: 0,0",
    "session.screen0.toolbar.onhead: 1",
    "session.screen0.toolbar.placement: BottomCenter",
]


@pytest.fixture(scope="module")
def heads():
    mgr = CompositorManager()
    mgr.start(config_set="default", headless=True, outputs=2,
              init_extra=INIT)
    yield mgr
    mgr.stop()


@pytest.fixture
def hipc(heads):
    client = FluxlandIPC(socket_path=heads.socket_path)
    yield client
    try:
        client.command("CloseAllWindows")
        time.sleep(0.3)
    except Exception:
        pass
    client.close()


def outputs(ipc):
    return {o["name"]: o for o in ipc.get_outputs()}


def open_window(ipc, title, extra=""):
    ipc.command(f"Exec foot {extra} --title {title}")
    deadline = time.monotonic() + 4
    while time.monotonic() < deadline:
        win = ipc.get_window_by_title(title)
        if win and win["width"] > 0:
            return win
        time.sleep(0.1)
    raise TimeoutError(title)


def frame_inside(win, box, slack=40):
    """The window's client area lies within box (frame decorations aside)."""
    return (box["x"] <= win["x"] and box["y"] <= win["y"] and
            win["x"] + win["width"] <= box["x"] + box["width"] and
            win["y"] + win["height"] <= box["y"] + box["height"] + slack)


def randr(heads, *args):
    exe = shutil.which("wlr-randr")
    if not exe:
        pytest.skip("wlr-randr is required")
    env = dict(os.environ, XDG_RUNTIME_DIR=heads.runtime_dir,
               WAYLAND_DISPLAY=heads.wayland_display)
    subprocess.run([exe, *args], env=env, check=True, timeout=5)
    time.sleep(SETTLE)


def test_output_settings_from_init(hipc):
    outs = outputs(hipc)
    one = outs["HEADLESS-1"]
    assert (one["width"], one["height"]) == (1024, 768)
    assert (one["x"], one["y"]) == (1280, 0)
    assert one["enabled"] is True
    assert (outs["HEADLESS-2"]["x"], outs["HEADLESS-2"]["y"]) == (0, 0)


def test_toolbar_reserves_space_on_its_head_only(hipc):
    outs = outputs(hipc)
    head1 = outs["HEADLESS-2"]   # announced first
    head2 = outs["HEADLESS-1"]
    assert head1["usable"]["height"] < head1["height"]
    assert head2["usable"] == head2["layout"]


def test_maximize_fills_its_own_head(hipc):
    outs = outputs(hipc)
    win = open_window(hipc, "MaxRight")
    hipc.command("MoveTo 1400 100", window=win["id"])
    hipc.command("Maximize", window=win["id"])
    time.sleep(SETTLE)
    win = hipc.get_window_by_title("MaxRight")
    usable = outs["HEADLESS-1"]["usable"]
    assert win["maximized"] is True
    assert win["x"] == usable["x"] and win["y"] == usable["y"]
    # The frame fits: client plus decorations no taller than the area
    assert usable["height"] - 60 < win["height"] < usable["height"]
    assert usable["width"] - 10 < win["width"] <= usable["width"]


def test_premap_maximize_request(hipc):
    win = open_window(hipc, "PreMax", "--maximized")
    assert win["maximized"] is True


def test_output_off_moves_windows_back(heads, hipc):
    win = open_window(hipc, "Stranded")
    hipc.command("MoveTo 1500 200", window=win["id"])
    hipc.command("Maximize", window=win["id"])
    time.sleep(SETTLE)

    randr(heads, "--output", "HEADLESS-1", "--off")
    outs = outputs(hipc)
    assert outs["HEADLESS-1"]["enabled"] is False
    win = hipc.get_window_by_title("Stranded")
    left = outs["HEADLESS-2"]
    # Now maximized on the remaining output
    assert win["maximized"] is True
    assert frame_inside(win, left["usable"])

    randr(heads, "--output", "HEADLESS-1", "--on", "--pos", "1280,0")
    assert outputs(hipc)["HEADLESS-1"]["enabled"] is True


def test_reconfigure_applies_output_scale(heads, hipc):
    with open(os.path.join(heads.config_dir, "init"), "a") as f:
        f.write("session.output.HEADLESS-1.scale: 2\n")
    hipc.command("Reconfigure")
    time.sleep(SETTLE)
    one = outputs(hipc)["HEADLESS-1"]
    assert (one["layout"]["width"], one["layout"]["height"]) == (512, 384)
    assert one["scale"] == 2
