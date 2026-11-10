"""SnapStep from the keyboard: undoing a minimize while the modifier is held.

Windows' Super+Down minimizes a window, and Super+Up before Super is let go
restores it (the accidental minimize).  The keys are sent through XTest (what
xdotool does) to the compositor's input emulation, which holds the modifier
across both.
"""

import os
import shutil
import subprocess
import time

import pytest

from lib.compositor import CompositorManager
from lib.ipc_client import FluxlandIPC

SETTLE = 0.6

pytestmark = pytest.mark.skipif(
    not (shutil.which("xdotool") and shutil.which("foot")
         and shutil.which("Xwayland")),
    reason="needs xdotool, foot and Xwayland")

INIT = [
    "session.screen0.toolbar.visible: false",
    "session.xwayland.inputEmulation: allow",
]

KEYS = (
    "Mod4 Down :SnapStep down\n"
    "Mod4 Up :SnapStep up\n"
    "Mod4 Shift Up :SnapStep up\n"
    "Mod1 Down :SnapStep down\n"
)


@pytest.fixture(scope="module")
def comp():
    mgr = CompositorManager()
    mgr.start(config_set="default", headless=True, outputs=1,
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
        client.command("CloseAllWindows")
        time.sleep(0.3)
    except Exception:
        pass
    client.close()


@pytest.fixture(scope="module")
def x_display(comp, tmp_path_factory):
    path = tmp_path_factory.mktemp("xdisplay") / "display"
    client = FluxlandIPC(socket_path=comp.socket_path)
    client.command(f"Exec sh -c 'echo $DISPLAY > {path}'")
    client.close()
    deadline = time.monotonic() + 5
    display = None
    while time.monotonic() < deadline:
        if path.exists() and path.read_text().strip():
            display = path.read_text().strip()
            break
        time.sleep(0.1)
    if not display:
        raise TimeoutError("no DISPLAY")
    # XWayland starts when something first connects: let it, so that the
    # first test's xdotool does not meet it half started
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        probe = subprocess.run(["xdotool", "getmouselocation"],
                               env={**comp.client_env, "DISPLAY": display},
                               capture_output=True)
        if probe.returncode == 0:
            break
        time.sleep(0.3)
    time.sleep(SETTLE)
    return display


def open_window(ipc, app_id):
    ipc.command(f"Exec foot --app-id {app_id}")
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        win = ipc.get_window_by_app_id(app_id)
        if win and win["width"] > 0:
            time.sleep(SETTLE)
            return ipc.get_window_by_app_id(app_id)
        time.sleep(0.1)
    raise TimeoutError(app_id)


def keys(comp, x_display, *actions):
    """xdotool actions, e.g. ("keydown", "Super_L", "key", "Down")"""
    subprocess.run(["xdotool", *actions],
                   env={**comp.client_env, "DISPLAY": x_display},
                   check=True, capture_output=True, timeout=10)
    time.sleep(SETTLE)


def state(ipc, app_id):
    return ipc.get_window_by_app_id(app_id)


def test_the_keys_reach_the_compositor(comp, ipc, x_display):
    open_window(ipc, "KeysWork")
    keys(comp, x_display, "keydown", "Super_L", "key", "Down", "keyup",
         "Super_L")
    assert state(ipc, "KeysWork")["minimized"] is True


def test_up_with_the_modifier_still_held_undoes_the_minimize(comp, ipc,
                                                             x_display):
    open_window(ipc, "Undo")
    keys(comp, x_display, "keydown", "Super_L", "key", "Down", "key", "Up",
         "keyup", "Super_L")
    assert state(ipc, "Undo")["minimized"] is False


def test_up_after_the_modifier_was_let_go_does_not(comp, ipc, x_display):
    open_window(ipc, "NoUndo")
    keys(comp, x_display, "keydown", "Super_L", "key", "Down", "keyup",
         "Super_L")
    assert state(ipc, "NoUndo")["minimized"] is True
    keys(comp, x_display, "keydown", "Super_L", "key", "Up", "keyup",
         "Super_L")
    # a new Super press: Up has no window to act on (it is minimized, and
    # nothing else is focused), so it stays minimized
    assert state(ipc, "NoUndo")["minimized"] is True


def test_the_undo_works_with_extra_modifiers_on_up(comp, ipc, x_display):
    open_window(ipc, "Extra")
    keys(comp, x_display, "keydown", "Super_L", "key", "Down",
         "keydown", "Shift_L", "key", "Up", "keyup", "Shift_L", "keyup",
         "Super_L")
    assert state(ipc, "Extra")["minimized"] is False


def test_a_different_modifier_does_not_carry_the_undo(comp, ipc, x_display):
    open_window(ipc, "Other")
    # minimized with Alt (Mod1), then Up with Super: Alt was let go, which
    # is what the undo needs to stay held
    keys(comp, x_display, "keydown", "Alt_L", "key", "Down", "keyup",
         "Alt_L")
    assert state(ipc, "Other")["minimized"] is True
    keys(comp, x_display, "keydown", "Super_L", "key", "Up", "keyup",
         "Super_L")
    assert state(ipc, "Other")["minimized"] is True


def test_nothing_else_is_stepped_while_the_minimize_can_be_undone(
        comp, ipc, x_display):
    # two windows: the second one, minimized, leaves the first focused
    open_window(ipc, "Below")
    open_window(ipc, "Minimized")
    before = state(ipc, "Below")["frame"]

    # one xdotool run, so that the modifier stays down through all of it
    start = time.monotonic()
    proc = subprocess.Popen(
        ["xdotool", "keydown", "Super_L", "key", "Down", "sleep", "1.5",
         "key", "Down", "key", "Left", "key", "Right", "sleep", "1.5",
         "key", "Up", "sleep", "1.5", "keyup", "Super_L"],
        env={**comp.client_env, "DISPLAY": x_display})
    try:
        time.sleep(0.9 - (time.monotonic() - start))
        assert state(ipc, "Minimized")["minimized"] is True
        # more keys with the modifier still held are about the minimized
        # window, not the one that has the focus now
        time.sleep(2.4 - (time.monotonic() - start))
        assert state(ipc, "Below")["minimized"] is False
        assert state(ipc, "Below")["frame"] == before
        assert state(ipc, "Minimized")["minimized"] is True
        # and up still takes the minimize back
        time.sleep(3.9 - (time.monotonic() - start))
        assert state(ipc, "Minimized")["minimized"] is False
    finally:
        proc.wait(timeout=10)

    # with the modifier let go, a new press is about the focused window
    keys(comp, x_display, "keydown", "Super_L", "key", "Down", "keyup",
         "Super_L")
    assert state(ipc, "Minimized")["minimized"] is True
