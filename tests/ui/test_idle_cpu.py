"""An idle compositor is idle.

Two loops used to keep fluxland busy with nothing going on: its handling of
a layer-shell client that draws again on every configure (swaybg) -- each
commit made it arrange the layer and configure the client, which committed
again -- and its D-Bus connection for the tray, which it watched for being
writable all the time (a socket nearly always is).  These look at the CPU
time the compositor and a wallpaper client use while nothing happens.
"""

import os
import shutil
import subprocess
import time

import pytest

from lib.compositor import CompositorManager

pytestmark = pytest.mark.skipif(
    not shutil.which("swaybg"), reason="swaybg is required")

TICKS = os.sysconf("SC_CLK_TCK")
WINDOW = 3.0		# seconds measured
BUSY = 0.15		# of one CPU: far above idle, far below the loops


def cpu_seconds(pid):
    with open(f"/proc/{pid}/stat") as f:
        fields = f.read().rsplit(")", 1)[1].split()
    return (int(fields[11]) + int(fields[12])) / TICKS


def busy_fraction(pids):
    start = {pid: cpu_seconds(pid) for pid in pids}
    time.sleep(WINDOW)
    return {pid: (cpu_seconds(pid) - start[pid]) / WINDOW for pid in pids}


@pytest.fixture(scope="module")
def comp():
    mgr = CompositorManager()
    mgr.start(config_set="default", headless=True, outputs=1)
    yield mgr
    mgr.stop()


def test_compositor_idle(comp):
    time.sleep(1.0)
    pid = comp._process.pid
    assert busy_fraction([pid])[pid] < BUSY


def test_compositor_and_wallpaper_idle_with_swaybg(comp):
    bg = subprocess.Popen(["swaybg", "-c", "#336699"], env=comp.client_env,
                          stdout=subprocess.DEVNULL,
                          stderr=subprocess.DEVNULL)
    try:
        time.sleep(2.0)
        used = busy_fraction([comp._process.pid, bg.pid])
        assert used[comp._process.pid] < BUSY, "compositor"
        assert used[bg.pid] < BUSY, "swaybg"
    finally:
        bg.terminate()
        bg.wait(timeout=5)
