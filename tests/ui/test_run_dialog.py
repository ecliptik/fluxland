"""RunDialog (fbrun's place): Return runs the line as a program, Tab
completes program and file names, Up and Down go through the history."""

import os
import shutil
import subprocess
import time

import pytest

from lib.compositor import CompositorManager
from lib.input_driver import InputDriver
from lib.ipc_client import FluxlandIPC

SETTLE = 0.4

pytestmark = pytest.mark.skipif(not shutil.which("wtype"),
                                reason="wtype is not installed")


@pytest.fixture(scope="module")
def comp(tmp_path_factory):
    state = tmp_path_factory.mktemp("state")
    work = tmp_path_factory.mktemp("work")
    mgr = CompositorManager()
    mgr.start(config_set="default", headless=True,
              extra_env={"XDG_STATE_HOME": str(state)})
    mgr.state_dir = state
    mgr.work_dir = work
    yield mgr
    mgr.stop()


@pytest.fixture
def ipc(comp):
    client = FluxlandIPC(socket_path=comp.socket_path)
    yield client
    client.close()


@pytest.fixture
def keys(comp):
    return InputDriver(env=comp.client_env)


def wait_for(path, timeout=5.0):
    end = time.time() + timeout
    while time.time() < end:
        if os.path.exists(path):
            return True
        time.sleep(0.05)
    return False


def test_return_runs_the_line(comp, ipc, keys):
    out = comp.work_dir / "ran"
    ipc.command("RunDialog")
    time.sleep(SETTLE)
    keys.type_text(f"touch {out}")
    keys.key("Return")
    assert wait_for(out)


def test_escape_runs_nothing(comp, ipc, keys):
    out = comp.work_dir / "not-run"
    ipc.command("RunDialog")
    time.sleep(SETTLE)
    keys.type_text(f"touch {out}")
    keys.key("Escape")
    time.sleep(SETTLE)
    assert not out.exists()


def test_tab_completes_a_file_name(comp, ipc, keys):
    (comp.work_dir / "uniquely-named-file").write_text("")
    out = comp.work_dir / "copied"
    ipc.command("RunDialog")
    time.sleep(SETTLE)
    keys.type_text(f"cp {comp.work_dir}/uniq")
    keys.key("Tab")
    keys.type_text(f" {out}")
    keys.key("Return")
    assert wait_for(out)


def test_history_is_kept_and_recalled(comp, ipc, keys):
    first = comp.work_dir / "hist-first"
    ipc.command("RunDialog")
    time.sleep(SETTLE)
    keys.type_text(f"touch {first}")
    keys.key("Return")
    assert wait_for(first)
    first.unlink()
    history = comp.state_dir / "fluxland" / "run_history"
    assert history.read_text().splitlines()[-1] == f"touch {first}"

    # Up brings the line back
    ipc.command("RunDialog")
    time.sleep(SETTLE)
    keys.key("Up")
    keys.key("Return")
    assert wait_for(first)


def dialog_windows(ipc):
    return [w for w in ipc.get_windows()
            if w.get("app_id") == "fluxland-run"]


def test_it_is_a_window_with_the_title(ipc, keys):
    ipc.command("RunDialog")
    time.sleep(SETTLE)
    wins = dialog_windows(ipc)
    assert len(wins) == 1
    assert wins[0]["title"] == "Run program"
    keys.key("Escape")
    time.sleep(SETTLE)
    assert dialog_windows(ipc) == []


def test_closing_the_window_closes_the_dialog(ipc, keys):
    ipc.command("RunDialog")
    time.sleep(SETTLE)
    assert len(dialog_windows(ipc)) == 1
    ipc.command("Close")
    time.sleep(SETTLE)
    assert dialog_windows(ipc) == []
    # and a new one opens
    ipc.command("RunDialog")
    time.sleep(SETTLE)
    assert len(dialog_windows(ipc)) == 1
    keys.key("Escape")
    time.sleep(SETTLE)


def test_a_held_key_repeats(comp, ipc, keys):
    # Repeating is the client's: the compositor tells it how fast
    ipc.command("RunDialog")
    time.sleep(SETTLE)
    keys.type_text(f"touch {comp.work_dir}/held-")
    subprocess.run(["wtype", "-P", "x", "-s", "1500", "-p", "x"],
                   env=comp.client_env, check=True)
    keys.key("Return")
    time.sleep(SETTLE)
    names = [p.name for p in comp.work_dir.glob("held-x*")]
    assert names and len(names[0]) - len("held-") >= 5, names
