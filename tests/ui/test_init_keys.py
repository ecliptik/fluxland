"""Fluxbox init keys that act on new windows: defaultDeco (which [Deco]
overrides)."""

import os
import time

from lib.compositor import CompositorManager
from lib.ipc_client import FluxlandIPC

SETTLE = 0.4


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


def test_default_deco_and_apps_override():
    mgr = CompositorManager()
    mgr.start(config_set="default", headless=True,
              init_extra=["session.screen0.defaultDeco: NONE"])
    try:
        with open(os.path.join(mgr.config_dir, "apps"), "w") as f:
            f.write("[app] (class=decoKept)\n  [Deco] {NORMAL}\n[end]\n")
        ipc = FluxlandIPC(socket_path=mgr.socket_path)
        # NONE: the frame is the client, no titlebar or border
        plain = open_window(ipc, "decoDefault")
        assert plain["frame"]["height"] == plain["height"]
        assert plain["frame"]["width"] == plain["width"]
        kept = open_window(ipc, "decoKept")
        assert kept["frame"]["height"] > kept["height"]
        ipc.close()
    finally:
        mgr.stop()
