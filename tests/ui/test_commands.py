"""Fluxbox commands: SetXProp and @NAME patterns, ClientPatternTest,
Attach, SetFocusProtection, RelabelButton, SaveRC, Deiconify's modes and
destinations, MarkWindow/GotoMarkedWindow with Arg."""

import os
import shutil
import time

import pytest

from lib.compositor import CompositorManager
from lib.input_driver import InputDriver
from lib.ipc_client import FluxlandIPC

SETTLE = 0.4

KEYS = """\
Mod4 m Arg :MarkWindow
Mod4 g Arg :GotoMarkedWindow
"""


@pytest.fixture(scope="module")
def comp():
    mgr = CompositorManager()
    mgr.start(config_set="default", headless=True, init_extra=[
        "session.screen0.toolbar.tools: workspacename, button.hello, clock",
        "session.screen0.toolbar.button.hello.label: Hi",
        "session.screen0.toolbar.button.hello.commands: Exec true",
    ])
    with open(os.path.join(mgr.config_dir, "keys"), "w") as f:
        f.write(KEYS)
    client = FluxlandIPC(socket_path=mgr.socket_path)
    client.command("Reconfigure")
    client.close()
    time.sleep(SETTLE)
    yield mgr
    mgr.stop()


@pytest.fixture
def cipc(comp):
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


def result(ipc, action, window=None):
    request = {"command": "command", "action": action}
    if window is not None:
        request["window"] = window
    reply = ipc._send(request)
    return reply.get("result")


def current_workspace(ipc):
    for ws in ipc.get_workspaces():
        if ws.get("focused"):
            return ws["index"]


def test_set_xprop_and_pattern(cipc):
    a = open_window(cipc, "propA")
    open_window(cipc, "propB")
    cipc.command("SetXProp MYTAG=hello", window=a["id"])
    out = result(cipc, "ClientPatternTest (@MYTAG=hello)")
    assert out.startswith("0x%x\t" % a["id"])
    assert out.count("\n") == 1
    assert result(cipc, "ClientPatternTest (@MYTAG=nope)") == "0\n"
    # The pattern works wherever patterns do
    cipc.command("If {Matches (@MYTAG=hello)} {Shade}", window=a["id"])
    time.sleep(SETTLE)
    assert cipc.get_window_by_app_id("propA")["shaded"] is True


def test_attach(cipc):
    open_window(cipc, "attach1")
    open_window(cipc, "attach2")
    open_window(cipc, "other")
    cipc.command("Attach (class=attach.*)")
    time.sleep(SETTLE)
    one = cipc.get_window_by_app_id("attach1")
    two = cipc.get_window_by_app_id("attach2")
    assert one["tab_group"] != 0 and one["tab_group"] == two["tab_group"]
    assert cipc.get_window_by_app_id("other")["tab_group"] == 0


def test_set_focus_protection_lock(cipc):
    a = open_window(cipc, "lockA")
    cipc.command("SetFocusProtection Lock", window=a["id"])
    open_window(cipc, "lockB")
    assert cipc.get_window_by_app_id("lockA")["focused"]
    # Nothing given is no command
    assert cipc._send({"command": "command",
                       "action": "SetFocusProtection"})["success"] is False


def test_relabel_button_and_save_rc(cipc):
    assert cipc.command("RelabelButton button.hello Bye") is not None
    reply = cipc._send({"command": "command",
                        "action": "RelabelButton button.nothere Bye"})
    assert reply["success"] is False
    assert cipc._send({"command": "command",
                       "action": "SaveRC"})["success"] is True


def test_deiconify_destinations(cipc):
    a = open_window(cipc, "iconA")
    cipc.command("Iconify", window=a["id"])
    time.sleep(SETTLE)
    # Current (the default): it comes to the current workspace
    cipc.command("Workspace 2")
    time.sleep(SETTLE)
    cipc.command("Deiconify Last")
    time.sleep(SETTLE)
    a = cipc.get_window_by_app_id("iconA")
    assert a["minimized"] is False and a["workspace"] == 1
    # OriginQuiet: back on its own workspace, without going there
    cipc.command("Iconify", window=a["id"])
    time.sleep(SETTLE)
    cipc.command("Workspace 1")
    time.sleep(SETTLE)
    cipc.command("Deiconify Last OriginQuiet")
    time.sleep(SETTLE)
    a = cipc.get_window_by_app_id("iconA")
    assert a["minimized"] is False and a["workspace"] == 1
    assert current_workspace(cipc) == 0
    # Origin: going there; LastWorkspace only looks here
    cipc.command("Iconify", window=a["id"])
    time.sleep(SETTLE)
    cipc.command("Deiconify LastWorkspace")
    time.sleep(SETTLE)
    assert cipc.get_window_by_app_id("iconA")["minimized"] is True
    cipc.command("Deiconify Last Origin")
    time.sleep(SETTLE)
    assert cipc.get_window_by_app_id("iconA")["minimized"] is False
    assert current_workspace(cipc) == 1


@pytest.mark.skipif(not shutil.which("wtype"), reason="wtype is needed")
def test_mark_and_goto_marked_window(comp, cipc):
    keys = InputDriver(env=comp.client_env)
    open_window(cipc, "markA")
    keys.key("m", ["super"])
    keys.key("1")
    time.sleep(SETTLE)
    open_window(cipc, "markB")
    assert cipc.get_window_by_app_id("markB")["focused"]
    keys.key("g", ["super"])
    keys.key("1")
    time.sleep(SETTLE)
    assert cipc.get_window_by_app_id("markA")["focused"]


def test_arrange_windows_with_a_pattern(cipc):
    a = open_window(cipc, "arrA")
    b = open_window(cipc, "arrB")
    c = open_window(cipc, "notArr")
    before = cipc.get_window_by_app_id("notArr")["frame"]
    cipc.command("ArrangeWindowsVertical (class=arr.*)")
    time.sleep(SETTLE * 2)
    usable = cipc.get_outputs()[0]["usable"]
    fa = cipc.get_window_by_app_id("arrA")["frame"]
    fb = cipc.get_window_by_app_id("arrB")["frame"]
    # Side by side, the whole height; the other left alone
    assert fa["height"] == usable["height"] == fb["height"]
    assert fa["x"] != fb["x"]
    assert cipc.get_window_by_app_id("notArr")["frame"] == before


def test_unclutter_with_a_pattern(cipc):
    open_window(cipc, "unclA")
    open_window(cipc, "unclB")
    for app in ("unclA", "unclB"):
        w = cipc.get_window_by_app_id(app)
        cipc.command("ResizeTo 300 200", window=w["id"])
        cipc.command("MoveTo 100 100", window=w["id"])
    time.sleep(SETTLE)
    cipc.command("Unclutter (class=uncl.*)")
    time.sleep(SETTLE)
    fa = cipc.get_window_by_app_id("unclA")["frame"]
    fb = cipc.get_window_by_app_id("unclB")["frame"]
    # Apart now: they don't overlap
    assert (fa["x"] + fa["width"] <= fb["x"] or
            fb["x"] + fb["width"] <= fa["x"] or
            fa["y"] + fa["height"] <= fb["y"] or
            fb["y"] + fb["height"] <= fa["y"])


def test_bind_key_writes_the_keys_file(comp, cipc):
    cipc.command("BindKey Mod4 F9 :SetWorkspaceName Bound")
    with open(os.path.join(comp.config_dir, "keys")) as f:
        assert f.read().endswith("Mod4 F9 :SetWorkspaceName Bound\n")
    reply = cipc._send({"command": "command",
                        "action": "BindKey not a binding"})
    assert reply["success"] is False
    if shutil.which("wtype"):
        InputDriver(env=comp.client_env).key("F9", ["super"])
        time.sleep(SETTLE)
        names = [ws["name"] for ws in cipc.get_workspaces()]
        assert "Bound" in names
        cipc.command("SetWorkspaceName One")


@pytest.mark.skipif(not shutil.which("wlrctl"), reason="wlrctl is needed")
def test_mouse_bindings_of_a_key_mode(comp, cipc):
    cipc.command("BindKey clicks: OnDesktop Mouse3 :SetWorkspaceName Clicked")
    pointer = InputDriver(env=comp.client_env)
    out = cipc.get_outputs()[0]
    spot = (out["x"] + 40, out["y"] + 40)
    # Not in the default mode
    cipc.command("SetWorkspaceName One")
    pointer.click(*spot, button=3)
    time.sleep(SETTLE)
    cipc.command("HideMenus")
    assert cipc.get_workspaces()[0]["name"] == "One"
    cipc.command("KeyMode clicks")
    pointer.click(*spot, button=3)
    time.sleep(SETTLE)
    assert cipc.get_workspaces()[0]["name"] == "Clicked"
    cipc.command("KeyMode default")
    cipc.command("SetWorkspaceName One")


def test_autotab_placement():
    mgr = CompositorManager()
    mgr.start(config_set="default", headless=True, init_extra=[
        "session.screen0.windowPlacement: AutotabPlacement"])
    try:
        ipc = FluxlandIPC(socket_path=mgr.socket_path)
        open_window(ipc, "autoA")
        open_window(ipc, "autoB")
        a = ipc.get_window_by_app_id("autoA")
        b = ipc.get_window_by_app_id("autoB")
        assert a["tab_group"] != 0 and a["tab_group"] == b["tab_group"]
        ipc.close()
    finally:
        mgr.stop()
