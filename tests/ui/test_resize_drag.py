"""Resizing a window by dragging its grip (OnRightGrip Move1
:StartResizing NearestCorner, as in the default keys): the dragged corner
follows the pointer, nothing jumps."""

import os
import time

import pytest

from lib.compositor import CompositorManager
from lib.input_driver import InputDriver, _find_test_pointer
from lib.ipc_client import FluxlandIPC

SETTLE = 0.4

pytestmark = pytest.mark.skipif(
    not _find_test_pointer(), reason="fluxland-test-pointer is not built")


@pytest.fixture(scope="module")
def comp():
    mgr = CompositorManager()
    mgr.start(config_set="default", headless=True, init_extra=[
        "session.screen0.opaqueResize: true",
        "session.screen0.edgeResizeSnapThreshold: 0",
    ])
    with open(os.path.join(mgr.config_dir, "keys"), "w") as f:
        f.write("OnRightGrip Move1 :StartResizing NearestCorner\n")
    client = FluxlandIPC(socket_path=mgr.socket_path)
    client.command("Reconfigure")
    client.close()
    time.sleep(SETTLE)
    yield mgr
    mgr.stop()


def test_grip_drag_follows_the_pointer(comp):
    ipc = FluxlandIPC(socket_path=comp.socket_path)
    try:
        ipc.command("Exec foot --app-id gripResize -W 50x10")
        deadline = time.monotonic() + 4
        win = None
        while time.monotonic() < deadline:
            win = ipc.get_window_by_app_id("gripResize")
            if win and win["width"] > 0:
                break
            time.sleep(0.1)
        assert win, "the window did not open"
        ipc.command("MoveTo 40 40")
        time.sleep(SETTLE)
        before = ipc.get_window_by_app_id("gripResize")["frame"]

        # The bottom-right grip; the drag's first move starts the resize
        # (Move1), its second (80, 50) resizes
        gx = before["x"] + before["width"] - 3
        gy = before["y"] + before["height"] - 2
        InputDriver(env=comp.client_env).drag(gx, gy, gx + 160, gy + 100)
        time.sleep(SETTLE)
        after = ipc.get_window_by_app_id("gripResize")["frame"]

        # Within foot's character cell steps; the bug added the window's
        # own position, hundreds of pixels
        assert (after["x"], after["y"]) == (before["x"], before["y"])
        assert 70 <= after["width"] - before["width"] <= 90, (before, after)
        assert 30 <= after["height"] - before["height"] <= 70, (before, after)
    finally:
        ipc.command("CloseAllWindows")
        ipc.close()
