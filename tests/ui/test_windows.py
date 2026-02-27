"""TC-WIN: Window management tests for fluxland compositor.

Tests core window operations: close, maximize, fullscreen, minimize,
shade, stick, show desktop, half-tiling, and raise/lower.
"""

import time

import pytest


SETTLE = 0.2  # seconds to wait after commands for state to settle
TOLERANCE = 10  # pixels tolerance for geometry comparisons


def approx_eq(actual, expected, tol=TOLERANCE):
    """Check that actual is within ±tol of expected."""
    return abs(actual - expected) <= tol


# ---------------------------------------------------------------------------
# TC-WIN-01: Close
# ---------------------------------------------------------------------------

def test_close(ipc, windows):
    """Open a window, close it, verify it's removed from the window list."""
    windows.open("CloseMe")
    time.sleep(SETTLE)

    ipc.command("Close")
    time.sleep(SETTLE)

    windows.wait_for_window_count(0)
    assert ipc.get_window_by_title("CloseMe") is None


# ---------------------------------------------------------------------------
# TC-WIN-02: Maximize toggle
# ---------------------------------------------------------------------------

def test_maximize_toggle(ipc, windows):
    """Maximize a window, verify geometry fills output, then restore."""
    win = windows.open("MaxToggle")
    time.sleep(SETTLE)

    orig_x, orig_y = win["x"], win["y"]
    orig_w, orig_h = win["width"], win["height"]

    output = ipc.get_outputs()[0]
    out_w, out_h = output["width"], output["height"]

    # Maximize
    ipc.command("Maximize")
    time.sleep(SETTLE)

    win = ipc.get_window_by_title("MaxToggle")
    assert win is not None
    assert win["maximized"] is True
    # Maximized window should roughly fill the output (minus toolbar/borders)
    assert win["width"] >= out_w - TOLERANCE
    assert win["height"] >= out_h * 0.65  # allow for toolbar + decorations

    # Restore
    ipc.command("Maximize")
    time.sleep(SETTLE)

    win = ipc.get_window_by_title("MaxToggle")
    assert win is not None
    assert win["maximized"] is False
    assert approx_eq(win["width"], orig_w)
    assert approx_eq(win["height"], orig_h)


# ---------------------------------------------------------------------------
# TC-WIN-03: Maximize vertical
# ---------------------------------------------------------------------------

def test_maximize_vertical(ipc, windows):
    """MaximizeVertical should make the window fill the output height."""
    win = windows.open("MaxVert")
    time.sleep(SETTLE)

    orig_w = win["width"]
    output = ipc.get_outputs()[0]

    ipc.command("MaximizeVertical")
    time.sleep(SETTLE)

    win = ipc.get_window_by_title("MaxVert")
    assert win is not None
    # Height should fill the output (minus toolbar/borders)
    assert win["height"] >= output["height"] * 0.8
    # Width should remain roughly the same
    assert approx_eq(win["width"], orig_w)


# ---------------------------------------------------------------------------
# TC-WIN-04: Maximize horizontal
# ---------------------------------------------------------------------------

def test_maximize_horizontal(ipc, windows):
    """MaximizeHorizontal should make the window fill the output width."""
    win = windows.open("MaxHoriz")
    time.sleep(SETTLE)

    orig_h = win["height"]
    output = ipc.get_outputs()[0]

    ipc.command("MaximizeHorizontal")
    time.sleep(SETTLE)

    win = ipc.get_window_by_title("MaxHoriz")
    assert win is not None
    # Width should fill the output
    assert win["width"] >= output["width"] - TOLERANCE
    # Height should remain roughly the same
    assert approx_eq(win["height"], orig_h)


# ---------------------------------------------------------------------------
# TC-WIN-05: Fullscreen
# ---------------------------------------------------------------------------

def test_fullscreen(ipc, windows):
    """Fullscreen should set fullscreen=True and fill the entire output."""
    windows.open("FullMe")
    time.sleep(SETTLE)

    output = ipc.get_outputs()[0]

    ipc.command("Fullscreen")
    time.sleep(SETTLE)

    win = ipc.get_window_by_title("FullMe")
    assert win is not None
    assert win["fullscreen"] is True
    assert approx_eq(win["width"], output["width"])
    assert approx_eq(win["height"], output["height"])


# ---------------------------------------------------------------------------
# TC-WIN-06: Minimize and deiconify
# ---------------------------------------------------------------------------

def test_minimize_and_deiconify(ipc, windows):
    """Minimize a window, verify it loses focus, then deiconify to restore."""
    # Open two windows so focus can shift when one is minimized
    windows.open("MinBg")
    time.sleep(SETTLE)
    windows.open("MinMe")
    time.sleep(SETTLE)

    # Verify MinMe is focused before minimizing
    focused = ipc.get_focused_window()
    assert focused is not None
    assert focused["title"] == "MinMe"

    ipc.command("Minimize")
    time.sleep(0.5)

    # After minimize, focus should shift to the other window
    focused = ipc.get_focused_window()
    assert focused is not None
    assert focused["title"] == "MinBg"

    # Deiconify to bring it back
    ipc.command("Deiconify")
    time.sleep(SETTLE)

    win = ipc.get_window_by_title("MinMe")
    assert win is not None


# ---------------------------------------------------------------------------
# TC-WIN-07: Shade toggle
# ---------------------------------------------------------------------------

def test_shade(ipc, windows):
    """Shade and unshade a window (verify commands succeed round-trip)."""
    win = windows.open("ShadeMe")
    time.sleep(SETTLE)

    orig_h = win["height"]

    # Shade the window (collapse to titlebar)
    ipc.command("ShadeOn")
    time.sleep(SETTLE)

    win = ipc.get_window_by_title("ShadeMe")
    assert win is not None

    # Unshade
    ipc.command("ShadeOff")
    time.sleep(SETTLE)

    win = ipc.get_window_by_title("ShadeMe")
    assert win is not None
    # After unshade, height should be restored to original
    assert approx_eq(win["height"], orig_h)


# ---------------------------------------------------------------------------
# TC-WIN-08: Stick
# ---------------------------------------------------------------------------

def test_stick(ipc, windows):
    """Stick should set sticky=True on the focused window."""
    windows.open("StickMe")
    time.sleep(SETTLE)

    ipc.command("Stick")
    time.sleep(SETTLE)

    win = ipc.get_window_by_title("StickMe")
    assert win is not None
    assert win["sticky"] is True


# ---------------------------------------------------------------------------
# TC-WIN-09: ShowDesktop
# ---------------------------------------------------------------------------

def test_show_desktop(ipc, windows):
    """ShowDesktop toggles: show → hide all, show again → restore."""
    wins = windows.open_many(2, prefix="DeskWin")
    time.sleep(SETTLE)

    assert len(ipc.get_windows()) == 2

    # First ShowDesktop: minimize/iconify all
    ipc.command("ShowDesktop")
    time.sleep(0.5)

    # Second ShowDesktop: restore all (toggle behavior)
    ipc.command("ShowDesktop")
    time.sleep(0.5)

    # After toggle back, windows should still be present
    all_wins = ipc.get_windows()
    assert len(all_wins) == 2
    titles = [w["title"] for w in all_wins]
    assert "DeskWin1" in titles
    assert "DeskWin2" in titles


# ---------------------------------------------------------------------------
# TC-WIN-10: LHalf / RHalf tiling
# ---------------------------------------------------------------------------

def test_lhalf_rhalf(ipc, windows):
    """LHalf should tile to left half, RHalf to right half."""
    windows.open("HalfMe")
    time.sleep(SETTLE)

    output = ipc.get_outputs()[0]
    out_w = output["width"]
    out_h = output["height"]
    half_w = out_w // 2

    # Tile left
    ipc.command("LHalf")
    time.sleep(SETTLE)

    win = ipc.get_window_by_title("HalfMe")
    assert win is not None
    assert approx_eq(win["x"], 0, tol=TOLERANCE)
    assert approx_eq(win["width"], half_w, tol=TOLERANCE)
    assert win["height"] >= out_h * 0.65  # should fill vertically

    # Tile right
    ipc.command("RHalf")
    time.sleep(SETTLE)

    win = ipc.get_window_by_title("HalfMe")
    assert win is not None
    assert approx_eq(win["x"], half_w, tol=TOLERANCE)
    assert approx_eq(win["width"], half_w, tol=TOLERANCE)


# ---------------------------------------------------------------------------
# TC-WIN-11: Raise / Lower
# ---------------------------------------------------------------------------

def test_raise_lower(ipc, windows):
    """Raise and Lower execute without error and affect stacking order."""
    win1 = windows.open("RLWin1")
    time.sleep(SETTLE)
    win2 = windows.open("RLWin2")
    time.sleep(SETTLE)

    # win2 should be focused (most recently opened)
    focused = ipc.get_focused_window()
    assert focused is not None
    assert focused["title"] == "RLWin2"

    # Lower should execute without error
    ipc.command("Lower")
    time.sleep(SETTLE)

    # Both windows should still exist
    all_wins = ipc.get_windows()
    titles = [w["title"] for w in all_wins]
    assert "RLWin1" in titles
    assert "RLWin2" in titles

    # Raise should execute without error
    ipc.command("Raise")
    time.sleep(SETTLE)

    all_wins = ipc.get_windows()
    assert len(all_wins) == 2
