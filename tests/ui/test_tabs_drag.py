"""Fluxbox's tab dragging (StartTabbing, FluxboxWindow::attachTo) and the
init keys around it: an outline is dragged, the tab joins the window it
is dropped on, or leaves its frame for where it was dropped."""

import os
import shutil
import time

import pytest

from lib.compositor import CompositorManager
from lib.input_driver import InputDriver
from lib.ipc_client import FluxlandIPC

SETTLE = 0.4

pytestmark = pytest.mark.skipif(
    not shutil.which("wlrctl") or not shutil.which("wtype"),
    reason="wlrctl and wtype are needed")


def start(init_extra=()):
    mgr = CompositorManager()
    mgr.start(config_set="default", headless=True,
              init_extra=list(init_extra))
    with open(os.path.join(mgr.config_dir, "keys"), "w") as f:
        f.write("OnTitlebar Mouse2 :StartTabbing\n")
    client = FluxlandIPC(socket_path=mgr.socket_path)
    client.command("Reconfigure")
    client.close()
    time.sleep(SETTLE)
    return mgr


@pytest.fixture(scope="module")
def comp():
    mgr = start()
    yield mgr
    mgr.stop()


@pytest.fixture
def tipc(comp):
    client = FluxlandIPC(socket_path=comp.socket_path)
    yield client
    try:
        client.command("CloseAllWindows")
        deadline = time.monotonic() + 4
        while client.get_windows() and time.monotonic() < deadline:
            time.sleep(0.1)
    except Exception:
        pass
    client.close()


def open_window(ipc, app_id, x, y):
    ipc.command(f"Exec foot --app-id {app_id} -W 50x10")
    deadline = time.monotonic() + 4
    while time.monotonic() < deadline:
        win = ipc.get_window_by_app_id(app_id)
        if win and win["width"] > 0:
            time.sleep(SETTLE)
            ipc.command(f"MoveTo {x} {y}", window=win["id"])
            time.sleep(SETTLE)
            return ipc.get_window_by_app_id(app_id)
        time.sleep(0.1)
    raise TimeoutError(app_id)


def title_point(win):
    f = win["frame"]
    return f["x"] + f["width"] // 2, f["y"] + 6


def test_drop_on_a_window_tabs_it(comp, tipc):
    pointer = InputDriver(env=comp.client_env)
    a = open_window(tipc, "tabA", 20, 20)
    b = open_window(tipc, "tabB", 700, 300)
    bx, by = b["x"] + b["width"] // 2, b["y"] + b["height"] // 2
    pointer.drag(*title_point(a), bx, by, button=2)
    time.sleep(SETTLE)
    a = tipc.get_window_by_app_id("tabA")
    b = tipc.get_window_by_app_id("tabB")
    assert a["tab_group"] != 0 and a["tab_group"] == b["tab_group"]


def test_drop_on_the_desktop_detaches_it(comp, tipc):
    pointer = InputDriver(env=comp.client_env)
    a = open_window(tipc, "tabC", 20, 20)
    b = open_window(tipc, "tabD", 700, 300)
    pointer.drag(*title_point(a), b["x"] + 40, b["y"] + 40, button=2)
    time.sleep(SETTLE)
    assert tipc.get_window_by_app_id("tabC")["tab_group"] != 0
    # Drag the tab off, onto the desktop: it leaves, and goes there
    b = tipc.get_window_by_app_id("tabD")
    out = tipc.get_outputs()[0]
    pointer.drag(*title_point(b), out["x"] + 300, out["y"] + 600,
                 button=2)
    time.sleep(SETTLE)
    c = tipc.get_window_by_app_id("tabC")
    d = tipc.get_window_by_app_id("tabD")
    assert c["tab_group"] == 0 and d["tab_group"] == 0
    moved = [w for w in (c, d) if abs(w["frame"]["y"] - 600) < 60]
    assert moved


def test_attach_area_titlebar():
    mgr = start(["session.tabsAttachArea: Titlebar"])
    try:
        ipc = FluxlandIPC(socket_path=mgr.socket_path)
        pointer = InputDriver(env=mgr.client_env)
        a = open_window(ipc, "tabE", 20, 20)
        b = open_window(ipc, "tabF", 700, 300)
        # On its client area: not tabbed, moved there instead
        pointer.drag(*title_point(a), b["x"] + b["width"] // 2,
                     b["y"] + b["height"] // 2, button=2)
        time.sleep(SETTLE)
        assert ipc.get_window_by_app_id("tabE")["tab_group"] == 0
        # It went where it was dropped
        a = ipc.get_window_by_app_id("tabE")
        assert a["frame"]["x"] > 400
        # On its titlebar: tabbed
        ipc.command("MoveTo 20 20", window=a["id"])
        time.sleep(SETTLE)
        a = ipc.get_window_by_app_id("tabE")
        b = ipc.get_window_by_app_id("tabF")
        pointer.drag(*title_point(a), *title_point(b), button=2)
        time.sleep(SETTLE)
        assert ipc.get_window_by_app_id("tabE")["tab_group"] != 0
        ipc.close()
    finally:
        mgr.stop()


def test_no_focus_while_typing():
    mgr = start(["session.screen0.noFocusWhileTypingDelay: 4000"])
    try:
        ipc = FluxlandIPC(socket_path=mgr.socket_path)
        keys = InputDriver(env=mgr.client_env)
        open_window(ipc, "typeA", 20, 20)
        assert ipc.get_window_by_app_id("typeA")["focused"]
        keys.type_text("x")
        open_window(ipc, "typeB", 600, 300)
        # Its user is typing: the new window waits
        assert ipc.get_window_by_app_id("typeA")["focused"]
        assert not ipc.get_window_by_app_id("typeB")["focused"]
        # Return isn't typing: the next one gets the focus
        keys.key("Return")
        open_window(ipc, "typeC", 300, 100)
        assert ipc.get_window_by_app_id("typeC")["focused"]
        ipc.close()
    finally:
        mgr.stop()
