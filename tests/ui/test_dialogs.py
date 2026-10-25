"""Fluxbox's text dialogs: CommandDialog, SetWorkspaceNameDialog,
SetTitleDialog, typed into with wtype."""

import shutil
import time

import pytest

from lib.compositor import CompositorManager
from lib.input_driver import InputDriver
from lib.ipc_client import FluxlandIPC

SETTLE = 0.4

pytestmark = pytest.mark.skipif(not shutil.which("wtype"),
                                reason="wtype is not installed")


@pytest.fixture(scope="module")
def comp():
    mgr = CompositorManager()
    mgr.start(config_set="default", headless=True)
    yield mgr
    mgr.stop()


@pytest.fixture
def dipc(comp):
    client = FluxlandIPC(socket_path=comp.socket_path)
    client.command("Workspace 1")
    client.command("SetWorkspaceName One")
    yield client
    try:
        client.command("CloseAllWindows")
    except Exception:
        pass
    client.close()


@pytest.fixture
def keys(comp):
    return InputDriver(env=comp.client_env)


def workspace_name(ipc):
    for ws in ipc.get_workspaces():
        if ws.get("focused") or ws.get("active") or ws.get("current"):
            return ws["name"]
    raise AssertionError("no current workspace")


def test_command_dialog_runs_what_is_typed(dipc, keys):
    dipc.command("CommandDialog")
    time.sleep(SETTLE)
    keys.type_text("SetWorkspaceName Typed")
    keys.key("Return")
    time.sleep(SETTLE)
    assert workspace_name(dipc) == "Typed"


def test_escape_closes_without_running(dipc, keys):
    dipc.command("CommandDialog")
    time.sleep(SETTLE)
    keys.type_text("SetWorkspaceName Nope")
    keys.key("Escape")
    time.sleep(SETTLE)
    assert workspace_name(dipc) == "One"
    # The keys reach the compositor again: a second dialog works
    dipc.command("CommandDialog")
    time.sleep(SETTLE)
    keys.type_text("SetWorkspaceName Again")
    keys.key("Return")
    time.sleep(SETTLE)
    assert workspace_name(dipc) == "Again"


def test_workspace_name_dialog_edits_the_name(dipc, keys):
    dipc.command("SetWorkspaceNameDialog")
    time.sleep(SETTLE)
    # It starts with the name: BackSpace twice, then more
    keys.key("BackSpace")
    keys.key("BackSpace")
    keys.type_text("ce")
    keys.key("Return")
    time.sleep(SETTLE)
    assert workspace_name(dipc) == "Oce"


def test_tab_completes_a_command(dipc, keys):
    dipc.command("CommandDialog")
    time.sleep(SETTLE)
    keys.type_text("SetWorkspaceNa")
    keys.key("Tab")
    keys.type_text(" Done")
    keys.key("Return")
    time.sleep(SETTLE)
    assert workspace_name(dipc) == "Done"


def test_set_title_dialog_keeps_the_title(dipc, keys):
    dipc.command("Exec foot --title DialogWin")
    deadline = time.monotonic() + 4
    win = None
    while time.monotonic() < deadline and not win:
        win = dipc.get_window_by_title("DialogWin")
        time.sleep(0.1)
    assert win
    time.sleep(SETTLE)
    dipc.command("SetTitleDialog", window=win["id"])
    time.sleep(SETTLE)
    keys.key("u", ["ctrl"])
    keys.type_text("Renamed")
    keys.key("Return")
    time.sleep(SETTLE)
    titles = [w["title"] for w in dipc.get_windows()]
    assert "Renamed" in titles


def test_set_resource_value_is_saved_and_applied(comp, dipc):
    import os
    dipc.command("SetResourceValue session.screen0.toolbar.visible false")
    time.sleep(SETTLE)
    assert dipc.get_config()[
        "toolbar_visible"] is False
    with open(os.path.join(comp.config_dir, "init")) as f:
        assert "session.screen0.toolbar.visible:\tfalse" in f.read()
    dipc.command("SetResourceValue session.screen0.toolbar.visible true")
    time.sleep(SETTLE)
    assert dipc.get_config()[
        "toolbar_visible"] is True
