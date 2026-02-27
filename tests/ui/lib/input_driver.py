"""Input simulation using wtype (keyboard) and wlrctl (pointer)."""

import shutil
import subprocess
import time


# Map button numbers to wlrctl names
_BUTTON_MAP = {
    1: "left",
    2: "middle",
    3: "right",
}


class InputDriver:
    """Simulate keyboard and pointer input on Wayland via wtype and wlrctl."""

    def __init__(self):
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

        subprocess.run(cmd, check=True, capture_output=True)

    def type_text(self, text: str, delay_ms: int = 50):
        """Type a string of text.

        Args:
            text: The text to type.
            delay_ms: Delay in milliseconds between characters.
        """
        self._require_wtype()
        cmd = ["wtype", "-d", str(delay_ms), text]
        subprocess.run(cmd, check=True, capture_output=True)

    # ── Pointer (via wlrctl) ────────────────────────────────────────

    def move_to(self, x: int, y: int):
        """Move the pointer to absolute coordinates.

        Args:
            x: Absolute X coordinate.
            y: Absolute Y coordinate.
        """
        self._require_wlrctl()
        subprocess.run(
            ["wlrctl", "pointer", "move", str(x), str(y)],
            check=True,
            capture_output=True,
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
        )
        time.sleep(0.05)
        subprocess.run(
            ["wlrctl", "pointer", "click", btn_name],
            check=True,
            capture_output=True,
        )

    def drag(self, x1, y1, x2, y2, button=1):
        """Drag from (x1, y1) to (x2, y2).

        Moves to start, presses button, moves to end, releases button.

        Args:
            x1, y1: Start coordinates.
            x2, y2: End coordinates.
            button: Mouse button (1=left, 2=middle, 3=right).
        """
        self._require_wlrctl()
        btn_name = _BUTTON_MAP.get(button)
        if btn_name is None:
            raise ValueError(f"Unknown button {button}; expected 1, 2, or 3")

        self.move_to(x1, y1)
        # Press button down
        subprocess.run(
            ["wlrctl", "pointer", "click", btn_name, "--press"],
            check=True,
            capture_output=True,
        )
        # Move to destination
        self.move_to(x2, y2)
        # Release button
        subprocess.run(
            ["wlrctl", "pointer", "click", btn_name, "--release"],
            check=True,
            capture_output=True,
        )
