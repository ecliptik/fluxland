"""Fluxbox's key events: ChangeWorkspace, FocusIn/FocusOut and
MouseOver/MouseOut bindings in the keys file."""

import os
import shutil
import time

import pytest

from lib.compositor import CompositorManager
from lib.input_driver import InputDriver
from lib.ipc_client import FluxlandIPC

SETTLE = 0.4

KEYS = """\
ChangeWorkspace :SetWorkspaceName Visited
FocusIn :SetTitle In
FocusOut :SetTitle Out
MouseOver :SetTitle Over
OnToolbar MouseOver :SetWorkspaceName Toolbar
"""

needs_wlrctl = pytest.mark.skipif(not shutil.which("wlrctl"),
                                  reason="wlrctl is not installed")


@pytest.fixture(scope="module")
def comp():
    mgr = CompositorManager()
    mgr.start(config_set="default", headless=True)
    with open(os.path.join(mgr.config_dir, "keys"), "w") as f:
        f.write(KEYS)
    client = FluxlandIPC(socket_path=mgr.socket_path)
    client.command("Reconfigure")
    client.close()
    time.sleep(SETTLE)
    yield mgr
    mgr.stop()


@pytest.fixture
def eipc(comp):
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


def workspace_names(ipc):
    return [ws["name"] for ws in ipc.get_workspaces()]


def open_window(ipc, app_id):
    ipc.command(f"Exec foot --app-id {app_id}")
    deadline = time.monotonic() + 4
    while time.monotonic() < deadline:
        win = ipc.get_window_by_app_id(app_id)
        if win and win["width"] > 0:
            time.sleep(SETTLE)
            return ipc.get_window_by_app_id(app_id)
        time.sleep(0.1)
    raise TimeoutError(app_id)


def test_change_workspace_runs_after_the_switch(eipc):
    eipc.command("Workspace 3")
    time.sleep(SETTLE)
    # On the new workspace, as Fluxbox runs it once it has switched
    assert workspace_names(eipc)[2] == "Visited"


def test_focus_in_and_out(eipc):
    a = open_window(eipc, "evA")
    assert eipc.get_window_by_app_id("evA")["title"] == "In"
    b = open_window(eipc, "evB")
    assert eipc.get_window_by_app_id("evB")["title"] == "In"
    assert eipc.get_window_by_app_id("evA")["title"] == "Out"
    eipc.command("Focus", window=a["id"])
    time.sleep(SETTLE)
    assert eipc.get_window_by_app_id("evA")["title"] == "In"
    assert eipc.get_window_by_app_id("evB")["title"] == "Out"


@needs_wlrctl
def test_mouse_over_window_and_toolbar(comp, eipc):
    pointer = InputDriver(env=comp.client_env)
    win = open_window(eipc, "evC")
    eipc.command("Workspace 1")
    pointer.move_to(win["x"] + win["width"] // 2,
                    win["y"] + win["height"] // 2)
    time.sleep(SETTLE)
    assert eipc.get_window_by_app_id("evC")["title"] == "Over"
    out = eipc.get_outputs()[0]
    # The toolbar is at the bottom of the output
    pointer.move_to(out["x"] + out["width"] // 2,
                    out["y"] + out["height"] - 3)
    time.sleep(SETTLE)
    assert workspace_names(eipc)[0] == "Toolbar"
