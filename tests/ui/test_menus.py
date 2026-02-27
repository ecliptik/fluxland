"""TC-MENU: Menu tests for fluxland compositor.

Tests menu open/close lifecycle via IPC commands and menu events.
Covers root menu, workspace menu, window menu, client menu, window list,
hide menus, and menu replacement behavior.
"""

import time

import pytest


SETTLE = 0.2  # seconds to wait after commands for state to settle


class TestMenuOpen:
    """Verify various menu types can be opened via IPC commands."""

    def test_root_menu_opens(self, ipc):
        """RootMenu triggers a menu_opened event."""
        stream = ipc.subscribe(["menu"])
        try:
            ipc.command("RootMenu")
            event = stream.wait_for("menu_opened", timeout=3.0)
            assert event is not None
            assert event.get("event") == "menu_opened"
        finally:
            stream.close()
            try:
                ipc.command("HideMenus")
            except Exception:
                pass

    def test_root_menu_has_title(self, ipc):
        """menu_opened event from RootMenu has a non-empty title."""
        stream = ipc.subscribe(["menu"])
        try:
            ipc.command("RootMenu")
            event = stream.wait_for("menu_opened", timeout=3.0)
            assert "title" in event
            assert isinstance(event["title"], str)
            assert len(event["title"]) > 0
        finally:
            stream.close()
            try:
                ipc.command("HideMenus")
            except Exception:
                pass

    def test_root_menu_has_items(self, ipc):
        """menu_opened event from RootMenu has items > 0."""
        stream = ipc.subscribe(["menu"])
        try:
            ipc.command("RootMenu")
            event = stream.wait_for("menu_opened", timeout=3.0)
            assert "items" in event
            assert isinstance(event["items"], int)
            assert event["items"] > 0
        finally:
            stream.close()
            try:
                ipc.command("HideMenus")
            except Exception:
                pass

    def test_workspace_menu_opens(self, ipc):
        """WorkspaceMenu triggers a menu_opened event."""
        stream = ipc.subscribe(["menu"])
        try:
            ipc.command("WorkspaceMenu")
            event = stream.wait_for("menu_opened", timeout=3.0)
            assert event is not None
            assert event.get("event") == "menu_opened"
        finally:
            stream.close()
            try:
                ipc.command("HideMenus")
            except Exception:
                pass

    def test_window_menu_opens(self, ipc, windows):
        """WindowMenu triggers menu_opened when a window is focused."""
        windows.open("MenuTarget")
        time.sleep(SETTLE)

        stream = ipc.subscribe(["menu"])
        try:
            ipc.command("WindowMenu")
            event = stream.wait_for("menu_opened", timeout=3.0)
            assert event is not None
            assert event.get("event") == "menu_opened"
        finally:
            stream.close()
            try:
                ipc.command("HideMenus")
            except Exception:
                pass

    def test_client_menu_opens(self, ipc):
        """ClientMenu triggers a menu_opened event."""
        stream = ipc.subscribe(["menu"])
        try:
            ipc.command("ClientMenu")
            event = stream.wait_for("menu_opened", timeout=3.0)
            assert event is not None
            assert event.get("event") == "menu_opened"
        finally:
            stream.close()
            try:
                ipc.command("HideMenus")
            except Exception:
                pass

    def test_window_list_opens(self, ipc):
        """WindowList triggers a menu_opened event."""
        stream = ipc.subscribe(["menu"])
        try:
            ipc.command("WindowList")
            event = stream.wait_for("menu_opened", timeout=3.0)
            assert event is not None
            assert event.get("event") == "menu_opened"
        finally:
            stream.close()
            try:
                ipc.command("HideMenus")
            except Exception:
                pass


class TestMenuClose:
    """Verify menu closing and replacement behavior."""

    def test_hide_menus_closes(self, ipc):
        """HideMenus after RootMenu triggers a menu_closed event."""
        stream = ipc.subscribe(["menu"])
        try:
            ipc.command("RootMenu")
            stream.wait_for("menu_opened", timeout=3.0)
            time.sleep(SETTLE)

            ipc.command("HideMenus")
            event = stream.wait_for("menu_closed", timeout=3.0)
            assert event is not None
            assert event.get("event") == "menu_closed"
        finally:
            stream.close()
            try:
                ipc.command("HideMenus")
            except Exception:
                pass

    def test_hide_menus_noop_when_none(self, ipc):
        """HideMenus with no menu open succeeds without error."""
        # Ensure no menus are open
        try:
            ipc.command("HideMenus")
        except Exception:
            pass
        time.sleep(SETTLE)

        # Should not raise
        response = ipc.command("HideMenus")
        assert response.get("success") is True

    def test_opening_new_menu_closes_previous(self, ipc):
        """Opening WorkspaceMenu while RootMenu is open closes root first."""
        stream = ipc.subscribe(["menu"])
        try:
            ipc.command("RootMenu")
            opened = stream.wait_for("menu_opened", timeout=3.0)
            root_title = opened.get("title", "")
            time.sleep(SETTLE)

            ipc.command("WorkspaceMenu")

            # Expect menu_closed for the root menu, then menu_opened for workspace
            closed = stream.wait_for("menu_closed", timeout=3.0)
            assert closed is not None
            assert closed.get("event") == "menu_closed"

            ws_opened = stream.wait_for("menu_opened", timeout=3.0)
            assert ws_opened is not None
            assert ws_opened.get("event") == "menu_opened"
        finally:
            stream.close()
            try:
                ipc.command("HideMenus")
            except Exception:
                pass
