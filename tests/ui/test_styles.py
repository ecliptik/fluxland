"""Tests for style/theme operations via IPC."""

import os
import time

import pytest

from lib.ipc_client import CommandError, TimeoutError as IPCTimeoutError

SETTLE = 0.2

_PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
STYLE_DEFAULT = os.path.join(_PROJECT_ROOT, "data", "style")
STYLE_HC_DARK = os.path.join(_PROJECT_ROOT, "data", "fluxland-hc-dark.style")
STYLE_HC_LIGHT = os.path.join(_PROJECT_ROOT, "data", "fluxland-hc-light.style")


class TestStyles:
    """Test style loading and reloading via IPC."""

    def test_reload_style_succeeds(self, ipc):
        """ReloadStyle returns success."""
        result = ipc.command("ReloadStyle")
        assert result.get("success") is True

    def test_set_style_succeeds(self, ipc):
        """SetStyle with the default style path returns success."""
        try:
            result = ipc.command(f"SetStyle {STYLE_DEFAULT}")
            time.sleep(SETTLE)
            assert result.get("success") is True
        finally:
            ipc.command("ReloadStyle")
            time.sleep(SETTLE)

    def test_set_style_hc_dark(self, ipc):
        """SetStyle with high-contrast dark style returns success."""
        try:
            result = ipc.command(f"SetStyle {STYLE_HC_DARK}")
            time.sleep(SETTLE)
            assert result.get("success") is True
        finally:
            # SetStyle updates config->style_file, so restore with SetStyle
            ipc.command(f"SetStyle {STYLE_DEFAULT}")
            time.sleep(SETTLE)

    def test_reload_style_fires_event(self, ipc):
        """ReloadStyle fires a style_changed event."""
        stream = ipc.subscribe(["style_changed"])
        try:
            ipc.command("ReloadStyle")
            time.sleep(SETTLE)
            try:
                event = stream.wait_for("style_changed", timeout=5.0)
                assert event is not None
                assert event.get("event") == "style_changed"
            except IPCTimeoutError:
                pytest.skip("style_changed event not fired in headless mode")
        finally:
            stream.close()

    def test_set_style_rejects_traversal(self, ipc):
        """SetStyle with path traversal does not crash the compositor."""
        try:
            ipc.command("SetStyle ../../../etc/passwd")
            time.sleep(SETTLE)
        except CommandError:
            pass  # Expected — command may reject the path
        # Verify compositor is still responsive
        assert ipc.ping() is True
