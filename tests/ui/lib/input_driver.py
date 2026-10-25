"""Input simulation using wtype (keyboard) and wlrctl (pointer)."""

import os
import shutil
import subprocess
import time
from pathlib import Path


# Map button numbers to wlrctl names
_BUTTON_MAP = {
    1: "left",
    2: "middle",
    3: "right",
}


def _find_test_pointer():
    """fluxland-test-pointer (tests/ui/tools/test_pointer.c), which holds
    a button across moves: FLUXLAND_TEST_POINTER, or beside the fluxland
    being tested, or in build/."""
    candidates = [os.environ.get("FLUXLAND_TEST_POINTER")]
    binary = os.environ.get("FLUXLAND_BINARY")
    if binary:
        candidates.append(str(Path(binary).parent / "tests" /
                              "fluxland-test-pointer"))
    root = Path(__file__).parent.parent.parent.parent
    candidates.append(str(root / "build" / "tests" /
                          "fluxland-test-pointer"))
    for path in candidates:
        if path and os.access(path, os.X_OK):
            return path
    return None


class InputDriver:
    """Simulate keyboard and pointer input on Wayland via wtype and wlrctl."""

    def __init__(self, env=None):
        """env: the environment for wtype/wlrctl (another compositor's
        WAYLAND_DISPLAY and XDG_RUNTIME_DIR); None uses this one's."""
        self.env = env
        """Detect available input tools."""
        self.has_wtype = shutil.which("wtype") is not None
        self.has_wlrctl = shutil.which("wlrctl") is not None

    def _require_wtype(self):
        """Raise RuntimeError if wtype is not available."""
        if not self.has_wtype:
            raise RuntimeError(
                "wtype is not installed or not in PATH; "
                "keyboard simulation is unavailable"
            )

    def _require_wlrctl(self):
        """Raise RuntimeError if wlrctl is not available."""
        if not self.has_wlrctl:
            raise RuntimeError(
                "wlrctl is not installed or not in PATH; "
                "pointer simulation is unavailable"
            )

    # ── Keyboard (via wtype) ────────────────────────────────────────

    def key(self, key: str, modifiers: list[str] | None = None):
        """Send a single key press, optionally with modifiers.

        Args:
            key: Key name (e.g. "Return", "Tab", "a").
            modifiers: List of modifier names (e.g. ["super", "shift"]).
                Valid modifiers: super, alt, ctrl, shift.

        Example:
            key("Return", ["super"])  →  wtype -M super -k Return -m super
        """
        self._require_wtype()
        cmd = ["wtype"]

        if modifiers:
            for mod in modifiers:
                cmd.extend(["-M", mod])

        cmd.extend(["-k", key])

        if modifiers:
            for mod in reversed(modifiers):
                cmd.extend(["-m", mod])

        subprocess.run(cmd, check=True, capture_output=True, env=self.env)

    def type_text(self, text: str, delay_ms: int = 50):
        """Type a string of text.

        Args:
            text: The text to type.
            delay_ms: Delay in milliseconds between characters.
        """
        self._require_wtype()
        cmd = ["wtype", "-d", str(delay_ms), text]
        subprocess.run(cmd, check=True, capture_output=True, env=self.env)

    # ── Pointer (via wlrctl) ────────────────────────────────────────

    def move_to(self, x: int, y: int):
        """Move the pointer to absolute coordinates.

        Args:
            x: Absolute X coordinate.
            y: Absolute Y coordinate.
        """
        self._require_wlrctl()
        # wlrctl moves by an offset: first into the top left corner (the
        # cursor stops at the layout's edge), then out to (x, y)
        for dx, dy in ((-100000, -100000), (x, y)):
            subprocess.run(
                ["wlrctl", "pointer", "move", str(dx), str(dy)],
                check=True,
                capture_output=True,
            env=self.env,
            )

    def click(self, x: int, y: int, button: int = 1):
        """Move pointer to (x, y) and click.

        Args:
            x: X coordinate.
            y: Y coordinate.
            button: Mouse button (1=left, 2=middle, 3=right).
        """
        self._require_wlrctl()
        btn_name = _BUTTON_MAP.get(button)
        if btn_name is None:
            raise ValueError(f"Unknown button {button}; expected 1, 2, or 3")

        self.move_to(x, y)
        subprocess.run(
            ["wlrctl", "pointer", "click", btn_name],
            check=True,
            capture_output=True,
            env=self.env,
        )

    def right_click(self, x: int, y: int):
        """Right-click at (x, y).

        Args:
            x: X coordinate.
            y: Y coordinate.
        """
        self.click(x, y, button=3)

    def double_click(self, x: int, y: int):
        """Double-click at (x, y).

        Args:
            x: X coordinate.
            y: Y coordinate.
        """
        self._require_wlrctl()
        self.move_to(x, y)
        btn_name = _BUTTON_MAP[1]
        subprocess.run(
            ["wlrctl", "pointer", "click", btn_name],
            check=True,
            capture_output=True,
            env=self.env,
        )
        time.sleep(0.05)
        subprocess.run(
            ["wlrctl", "pointer", "click", btn_name],
            check=True,
            capture_output=True,
            env=self.env,
        )

    def drag(self, x1, y1, x2, y2, button=1):
        """Drag from (x1, y1) to (x2, y2): press there, move, release.

        wlrctl only clicks, so this takes fluxland-test-pointer (built
        with the tests); without it the test is skipped.
        """
        import pytest
        helper = _find_test_pointer()
        if not helper:
            pytest.skip("fluxland-test-pointer is not built")
        subprocess.run(
            [helper, "move", str(x1), str(y1), "press", str(button),
             "sleep", "50", "to", str((x1 + x2) // 2), str((y1 + y2) // 2),
             "sleep", "50", "to", str(x2), str(y2), "sleep", "50",
             "release", str(button)],
            check=True,
            capture_output=True,
            env=self.env,
        )

    def scroll(self, dy: int, dx: int = 0):
        """Scroll the wheel: dy < 0 is up (Fluxbox's Mouse4), > 0 down."""
        self._require_wlrctl()
        subprocess.run(
            ["wlrctl", "pointer", "scroll", str(dy), str(dx)],
            check=True,
            capture_output=True,
            env=self.env,
        )
