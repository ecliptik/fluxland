"""Maximized windows moved or resized by hand.

Resizing un-maximizes (and keeps the size it has: Fluxbox's
disableMaximization), so restoring later returns to what was made by hand.
Moving keeps a maximized window maximized, on the head it is dropped on:
dropped on its own head it goes back; dropped on another it is maximized
there, and what restoring returns it to is on that head too.
maxDisableMove / maxDisableResize refuse both.

Two heads: HEADLESS-2 is head 1 at 0,0 with the toolbar, HEADLESS-1 the
second at 1280,0.
"""

import os
import time

import pytest

from lib.compositor import CompositorManager
from lib.input_driver import InputDriver, _find_test_pointer
from lib.ipc_client import FluxlandIPC

SETTLE = 0.5
STEP = 40  # a terminal's size comes in character cells

pytestmark = pytest.mark.skipif(
    not _find_test_pointer(), reason="fluxland-test-pointer is not built")

INIT = [
    "session.output.HEADLESS-1.mode: 1024x768",
    "session.output.HEADLESS-1.position: 1280,0",
    "session.output.HEADLESS-2.position: 0,0",
    "session.screen0.toolbar.onhead: 1",
    "session.screen0.toolbar.placement: BottomCenter",
    "session.screen0.opaqueResize: true",
    "session.screen0.opaqueMove: true",
    "session.screen0.edgeResizeSnapThreshold: 0",
    "session.screen0.edgeSnapThreshold: 0",
    # a drag to the edge of the layout would carry the window to the next
    # workspace
    "session.screen0.workspacewarping: false",
]

KEYS = (
    "OnRightGrip Move1 :StartResizing NearestCorner\n"
    "OnTitlebar Move1 :StartMoving\n"
)


@pytest.fixture(scope="module")
def comp():
    mgr = CompositorManager()
    mgr.start(config_set="default", headless=True, outputs=2,
              init_extra=INIT)
    with open(os.path.join(mgr.config_dir, "keys"), "w") as f:
        f.write(KEYS)
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
        client.command("SetResourceValue session.screen0.maxDisableMove "
                       "false")
        client.command("SetResourceValue session.screen0.maxDisableResize "
                       "false")
        client.command("SetResourceValue session.screen0.enableWindowSnapping "
                       "false")
        client.command("CloseAllWindows")
        time.sleep(0.3)
    except Exception:
        pass
    client.close()


def open_window(ipc, title):
    # By app id: the shell in the terminal changes its title
    ipc.command(f"Exec foot --app-id {title}")
    deadline = time.monotonic() + 4
    while time.monotonic() < deadline:
        win = ipc.get_window_by_app_id(title)
        if win and win["width"] > 0:
            return win
        time.sleep(0.1)
    raise TimeoutError(title)


def window(ipc, title):
    return ipc.get_window_by_app_id(title)


def open_on_head1(ipc, title):
    """A window on head 1, at a known place: it opens on the head the
    pointer is on, which the last test left wherever."""
    win = open_window(ipc, title)
    ipc.command("SetHead 1", window=win["id"])
    time.sleep(SETTLE)
    ipc.command("MoveTo 100 100", window=win["id"])
    time.sleep(SETTLE)
    return window(ipc, title)


def outputs(ipc):
    return {o["name"]: o for o in ipc.get_outputs()}


def maximized_window(ipc, title):
    """A window maximized on head 1, started from a known place inside its
    usable area (clear of the toolbar)."""
    win = open_on_head1(ipc, title)
    ipc.command("Maximize", window=win["id"])
    time.sleep(SETTLE)
    win = window(ipc, title)
    assert win["maximized"] is True
    return win


def titlebar_point(win):
    f = win["frame"]
    return f["x"] + f["width"] // 2, f["y"] + 8


def test_resize_unmaximizes_and_keeps_the_size(comp, ipc):
    win = maximized_window(ipc, "ResizeMax")
    before = win["frame"]

    gx = before["x"] + before["width"] - 3
    gy = before["y"] + before["height"] - 2
    InputDriver(env=comp.client_env).drag(gx, gy, gx - 200, gy - 120)
    time.sleep(SETTLE)

    after = window(ipc, "ResizeMax")
    assert after["maximized"] is False
    # Resized from the maximized size, its corner staying put (the first
    # move of a drag starts it: half the distance is what counts)
    assert (after["frame"]["x"], after["frame"]["y"]) == (
        before["x"], before["y"])
    assert 70 <= before["width"] - after["frame"]["width"] <= 130
    assert 30 <= before["height"] - after["frame"]["height"] <= 90

    # Maximize and restore: back to what was made by hand, not to where
    # it was before it was maximized (100, 100)
    made = after["frame"]
    ipc.command("Maximize", window=win["id"])
    time.sleep(SETTLE)
    assert window(ipc, "ResizeMax")["maximized"] is True
    ipc.command("Maximize", window=win["id"])
    time.sleep(SETTLE)
    restored = window(ipc, "ResizeMax")
    assert restored["maximized"] is False
    assert abs(restored["frame"]["width"] - made["width"]) <= STEP
    assert abs(restored["frame"]["x"] - made["x"]) <= STEP


def test_move_on_the_same_head_snaps_back(comp, ipc):
    win = maximized_window(ipc, "MoveSame")
    before = win["frame"]

    tx, ty = titlebar_point(win)
    InputDriver(env=comp.client_env).drag(tx, ty, tx + 60, ty + 40)
    time.sleep(SETTLE)

    after = window(ipc, "MoveSame")
    assert after["maximized"] is True
    assert after["frame"] == before


def test_move_to_another_head_follows(comp, ipc):
    outs = outputs(ipc)
    head2 = outs["HEADLESS-1"]["usable"]
    win = maximized_window(ipc, "MoveOther")

    tx, ty = titlebar_point(win)
    # (half of it counts; the window's centre ends on head 2, the pointer
    # short of the layout's edge)
    InputDriver(env=comp.client_env).drag(tx, ty, tx + 1500, ty + 30)
    time.sleep(SETTLE)

    after = window(ipc, "MoveOther")
    assert after["maximized"] is True
    f = after["frame"]
    assert (f["x"], f["y"]) == (head2["x"], head2["y"])
    assert abs(f["width"] - head2["width"]) <= 2

    # Restoring puts it on that head, not back on the first
    ipc.command("Maximize", window=win["id"])
    time.sleep(SETTLE)
    restored = window(ipc, "MoveOther")
    assert restored["maximized"] is False
    assert restored["frame"]["x"] >= head2["x"]


def test_pull_down_restores_the_size(comp, ipc):
    win = open_on_head1(ipc, "PullDown")
    original = window(ipc, "PullDown")["frame"]
    ipc.command("Maximize", window=win["id"])
    time.sleep(SETTLE)
    win = window(ipc, "PullDown")
    assert win["maximized"] is True

    # Mostly down (half of the distance counts: well over the threshold)
    tx, ty = titlebar_point(win)
    InputDriver(env=comp.client_env).drag(tx, ty, tx + 20, ty + 300)
    time.sleep(SETTLE)

    after = window(ipc, "PullDown")
    assert after["maximized"] is False
    assert abs(after["frame"]["width"] - original["width"]) <= STEP
    assert abs(after["frame"]["height"] - original["height"]) <= STEP
    # The pointer stays on it: near the middle across, as it was
    f = after["frame"]
    assert f["x"] < tx < f["x"] + f["width"]


def test_throw_sideways_is_not_a_pull(comp, ipc):
    win = maximized_window(ipc, "Throw")
    tx, ty = titlebar_point(win)
    # Far more sideways than down: it follows the head, still maximized
    InputDriver(env=comp.client_env).drag(tx, ty, tx + 1500, ty + 120)
    time.sleep(SETTLE)
    after = window(ipc, "Throw")
    assert after["maximized"] is True
    assert after["frame"]["x"] >= 1280


def test_snap_to_the_top_edge_maximizes_there(comp, ipc):
    outs = outputs(ipc)
    head2 = outs["HEADLESS-1"]["usable"]
    ipc.command("SetResourceValue session.screen0.enableWindowSnapping true")
    time.sleep(SETTLE)
    win = open_on_head1(ipc, "SnapTop")
    original = win["frame"]

    tx, ty = titlebar_point(win)
    # The top edge of head 2 (half the distance counts; the pointer ends
    # in the zone, away from the corners)
    InputDriver(env=comp.client_env).drag(tx, ty, head2["x"] + 500, 2)
    time.sleep(SETTLE)

    after = window(ipc, "SnapTop")
    assert after["maximized"] is True
    f = after["frame"]
    assert (f["x"], f["y"]) == (head2["x"], head2["y"])
    assert abs(f["width"] - head2["width"]) <= 2

    # Restoring goes back to the size it had
    ipc.command("Maximize", window=win["id"])
    time.sleep(SETTLE)
    restored = window(ipc, "SnapTop")
    assert restored["maximized"] is False
    assert abs(restored["frame"]["width"] - original["width"]) <= STEP


def test_max_disable_move_refuses(comp, ipc):
    win = maximized_window(ipc, "NoMove")
    ipc.command("SetResourceValue session.screen0.maxDisableMove true")
    time.sleep(SETTLE)
    before = window(ipc, "NoMove")["frame"]

    tx, ty = titlebar_point(win)
    InputDriver(env=comp.client_env).drag(tx, ty, tx + 1500, ty + 30)
    time.sleep(SETTLE)

    after = window(ipc, "NoMove")
    assert after["maximized"] is True
    assert after["frame"] == before


def test_max_disable_resize_refuses(comp, ipc):
    win = maximized_window(ipc, "NoResize")
    ipc.command("SetResourceValue session.screen0.maxDisableResize true")
    time.sleep(SETTLE)
    before = window(ipc, "NoResize")["frame"]

    gx = before["x"] + before["width"] - 3
    gy = before["y"] + before["height"] - 2
    InputDriver(env=comp.client_env).drag(gx, gy, gx - 200, gy - 120)
    time.sleep(SETTLE)

    after = window(ipc, "NoResize")
    assert after["maximized"] is True
    assert after["frame"] == before
