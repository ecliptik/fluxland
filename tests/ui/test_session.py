"""The desktop session environment: what portals and services see.

Runs its own headless compositor with a stand-in for
dbus-update-activation-environment on PATH that records its arguments.
"""

import os
import stat
import time

import pytest

from lib.compositor import CompositorManager
from lib.ipc_client import FluxlandIPC

FAKE = """#!/bin/sh
{ echo "$*"; } >> "$FAKE_LOG"
"""


def wait_for_lines(path, count, timeout=5.0):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if os.path.exists(path):
            with open(path) as f:
                lines = f.read().splitlines()
            if len(lines) >= count:
                return lines
        time.sleep(0.1)
    return open(path).read().splitlines() if os.path.exists(path) else []


@pytest.fixture
def session(tmp_path, monkeypatch):
    bindir = tmp_path / "bin"
    bindir.mkdir()
    fake = bindir / "dbus-update-activation-environment"
    fake.write_text(FAKE)
    fake.chmod(fake.stat().st_mode | stat.S_IXUSR)
    log = tmp_path / "activation.log"

    env = {
        "PATH": f"{bindir}:{os.environ.get('PATH', '')}",
        "FAKE_LOG": str(log),
    }
    for var in ("XDG_CURRENT_DESKTOP", "XDG_SESSION_TYPE"):
        monkeypatch.delenv(var, raising=False)
    mgr = CompositorManager()
    mgr.start(config_set="default", headless=True, extra_env=env,
              init_extra=["session.updateActivationEnvironment: true"])
    yield mgr, log
    mgr.stop()


def child_env(ipc, tmp_dir):
    """Environment of a process the compositor starts."""
    out = os.path.join(tmp_dir, "env.out")
    ipc.command(f"Exec env > {out}")
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        if os.path.exists(out) and os.path.getsize(out) > 0:
            time.sleep(0.1)
            with open(out) as f:
                return dict(line.split("=", 1) for line in
                            f.read().splitlines() if "=" in line)
        time.sleep(0.1)
    raise TimeoutError("env")


def test_children_get_session_variables(session, tmp_path):
    mgr, _ = session
    ipc = FluxlandIPC(socket_path=mgr.socket_path)
    try:
        env = child_env(ipc, str(tmp_path))
    finally:
        ipc.close()
    assert env["XDG_CURRENT_DESKTOP"] == "fluxland:wlroots"
    assert env["XDG_SESSION_TYPE"] == "wayland"
    assert env["WAYLAND_DISPLAY"] == mgr.wayland_display
    assert env["FLUXLAND_SOCK"] == mgr.socket_path


def test_activation_environment_set_and_cleared(session):
    mgr, log = session
    lines = wait_for_lines(str(log), 1)
    assert lines, "activation environment was not updated"
    first = lines[0].split()
    assert first[0] == "--systemd"
    assert "WAYLAND_DISPLAY" in first
    assert "XDG_CURRENT_DESKTOP" in first

    mgr.stop()
    lines = wait_for_lines(str(log), 2)
    assert len(lines) == 2, "not cleared at exit"
    assert "WAYLAND_DISPLAY=" in lines[1].split()
