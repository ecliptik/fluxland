"""Workspace management tests for fluxland compositor.

Tests workspace querying, creation, removal, renaming, and window-to-workspace
operations via IPC. Workspace switching (Workspace N) is skipped because it
does not work in headless mode.
"""

import time

import pytest


class TestWorkspaceQuery:
    """Verify workspace querying returns correct default state."""

    def test_default_workspace_count(self, ipc):
        """Verify 4 workspaces exist from the default config."""
        workspaces = ipc.get_workspaces()
        assert len(workspaces) == 4

    def test_default_workspace_names(self, ipc):
        """Verify default workspace names are One, Two, Three, Four."""
        workspaces = ipc.get_workspaces()
        names = [ws["name"] for ws in workspaces]
        assert names == ["One", "Two", "Three", "Four"]

    def test_workspace_structure(self, ipc):
        """Verify each workspace dict has index, name, focused, visible fields."""
        workspaces = ipc.get_workspaces()
        for ws in workspaces:
            assert "index" in ws, "Missing 'index' field"
            assert "name" in ws, "Missing 'name' field"
            assert "focused" in ws, "Missing 'focused' field"
            assert "visible" in ws, "Missing 'visible' field"
            assert isinstance(ws["index"], int)
            assert isinstance(ws["name"], str)
            assert isinstance(ws["focused"], bool)

    def test_active_workspace(self, ipc):
        """Verify exactly one workspace has focused=True."""
        workspaces = ipc.get_workspaces()
        focused = [ws for ws in workspaces if ws["focused"]]
        assert len(focused) == 1, (
            f"Expected exactly 1 focused workspace, got {len(focused)}"
        )


class TestWorkspaceManagement:
    """Verify workspace add/remove/rename operations."""

    def test_add_workspace(self, ipc):
        """AddWorkspace increases the workspace count by 1."""
        initial = ipc.get_workspaces()
        initial_count = len(initial)

        ipc.command("AddWorkspace")
        time.sleep(0.1)

        updated = ipc.get_workspaces()
        assert len(updated) == initial_count + 1

        # Cleanup
        ipc.command("RemoveLastWorkspace")
        time.sleep(0.1)

    def test_remove_last_workspace(self, ipc):
        """AddWorkspace then RemoveLastWorkspace restores the original count."""
        initial = ipc.get_workspaces()
        initial_count = len(initial)

        ipc.command("AddWorkspace")
        time.sleep(0.1)
        assert len(ipc.get_workspaces()) == initial_count + 1

        ipc.command("RemoveLastWorkspace")
        time.sleep(0.1)
        assert len(ipc.get_workspaces()) == initial_count

    def test_add_multiple_workspaces(self, ipc):
        """Add 3 workspaces, verify count, then remove all 3."""
        initial = ipc.get_workspaces()
        initial_count = len(initial)

        for _ in range(3):
            ipc.command("AddWorkspace")
            time.sleep(0.1)

        updated = ipc.get_workspaces()
        assert len(updated) == initial_count + 3

        for _ in range(3):
            ipc.command("RemoveLastWorkspace")
            time.sleep(0.1)

        final = ipc.get_workspaces()
        assert len(final) == initial_count

    def test_set_workspace_name(self, ipc):
        """SetWorkspaceName changes the current workspace's name."""
        active = ipc.get_active_workspace()
        original_name = active["name"]

        ipc.command("SetWorkspaceName Custom")
        time.sleep(0.1)

        updated = ipc.get_active_workspace()
        assert updated["name"] == "Custom"

        # Restore original name
        ipc.command(f"SetWorkspaceName {original_name}")
        time.sleep(0.1)

        restored = ipc.get_active_workspace()
        assert restored["name"] == original_name


class TestWindowWorkspaceOperations:
    """Verify moving windows between workspaces."""

    def test_send_to_workspace(self, ipc, windows):
        """SendToWorkspace moves the focused window to another workspace."""
        win = windows.open("SendTest")
        assert win is not None

        # The window starts on workspace index 0
        assert win["workspace"] == 0

        # SendToWorkspace uses 1-based numbering, so 2 = workspace index 1
        ipc.command("SendToWorkspace 2")
        time.sleep(0.2)

        # Re-query the window — it should now be on workspace index 1
        updated = ipc.get_window_by_title("SendTest")
        assert updated is not None, "Window disappeared after SendToWorkspace"
        assert updated["workspace"] == 1, (
            f"Expected window on workspace 1, got {updated['workspace']}"
        )

    def test_stick_window(self, ipc, windows):
        """Stick makes a window sticky (visible on all workspaces)."""
        win = windows.open("StickyTest")
        assert win is not None
        assert win.get("sticky") is not True, "Window should not start sticky"

        ipc.command("Stick")
        time.sleep(0.1)

        updated = ipc.get_window_by_title("StickyTest")
        assert updated is not None
        assert updated["sticky"] is True, "Window should be sticky after Stick"
