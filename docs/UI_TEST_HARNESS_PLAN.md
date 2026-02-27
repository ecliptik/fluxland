# Fluxland UI/UX Test Harness — Architecture Plan

## Executive Summary

This document describes a fully automated UI/UX testing harness for fluxland, a
Fluxbox-inspired Wayland compositor. The harness tests all major features against
expected Fluxbox behavior, interacts with a real Wayland desktop, captures
screenshots for visual regression, and produces developer-actionable bug reports.

The design leverages fluxland's **rich IPC system (80+ commands)** as the primary
automation primitive, combined with **grim** for screenshots, **wtype** for
keyboard input, **wlrctl** for pointer simulation, and **Python/Pillow** for
image comparison and orchestration.

---

## 1. Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                    Test Runner (Python)                       │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌─────────────┐ │
│  │ Test Case │  │ Test Case │  │ Test Case │  │  Report Gen │ │
│  │ Registry  │  │ Executor  │  │ Asserter  │  │  (HTML/MD)  │ │
│  └──────────┘  └──────────┘  └──────────┘  └─────────────┘ │
│        │              │              │              │         │
│  ┌─────┴──────────────┴──────────────┴──────────────┘       │
│  │              Automation Layer                              │
│  │  ┌──────────┐  ┌──────────┐  ┌───────────┐              │
│  │  │ IPC      │  │ Input    │  │ Screenshot │              │
│  │  │ Client   │  │ Driver   │  │ Capture    │              │
│  │  └────┬─────┘  └────┬─────┘  └─────┬─────┘              │
│  └───────┼──────────────┼──────────────┼────────────────────┘
│          │              │              │                      │
└──────────┼──────────────┼──────────────┼─────────────────────┘
           │              │              │
     Unix Socket     wtype/wlrctl      grim
           │              │              │
    ┌──────┴──────────────┴──────────────┴─────┐
    │           fluxland compositor             │
    │        (live session on VM display)       │
    └──────────────────────────────────────────┘
```

### Two Execution Modes

1. **Live Session Mode** (primary) — Tests run against fluxland on the VM's
   real display. Full visual testing with screenshots. Requires a running
   Wayland session.

2. **Headless Mode** (CI/fast) — Tests run against fluxland with
   `WLR_BACKENDS=headless WLR_RENDERER=pixman`. No screenshots, but full IPC
   and state verification. Suitable for CI pipelines.

---

## 2. Prerequisites & Tool Installation

### Already Installed
| Tool | Purpose |
|------|---------|
| `wtype` 0.4 | Keyboard input simulation |
| `grim` 1.4 | Screenshot capture (PNG) |
| `wl-clipboard` 2.2 | Clipboard testing (`wl-copy`/`wl-paste`) |
| `python3` + `Pillow` 11.1 | Test runner + image comparison |
| `PyGObject` | GLib/GTK bindings (available) |

### Must Install
```bash
sudo apt install wlrctl slurp imagemagick
# Optional but recommended:
sudo apt install cage wev wf-recorder
```

| Tool | Purpose | Priority |
|------|---------|----------|
| `wlrctl` 0.2 | Window listing, virtual pointer, window focus/close | **Required** |
| `slurp` 1.5 | Region selection for targeted screenshots | Recommended |
| `imagemagick` | CLI `compare` for quick diffs, `convert` for processing | Recommended |
| `cage` 0.2 | Isolated kiosk compositor for nested testing | Optional |
| `wev` | Event debugger for test development | Optional |
| `wf-recorder` | Record test sessions as video | Optional |

---

## 3. Directory Structure

```
tests/
└── ui/
    ├── conftest.py              # pytest fixtures (compositor, ipc, screenshot)
    ├── pytest.ini               # pytest configuration
    ├── requirements.txt         # Python dependencies (pytest, Pillow, etc.)
    │
    ├── lib/                     # Automation library
    │   ├── __init__.py
    │   ├── ipc_client.py        # Fluxland IPC JSON client
    │   ├── input_driver.py      # wtype/wlrctl wrapper
    │   ├── screenshot.py        # grim capture + Pillow comparison
    │   ├── compositor.py        # Compositor lifecycle (start/stop/config)
    │   ├── wayland_client.py    # Minimal Wayland client for creating test surfaces
    │   └── report.py            # Bug report / test result generation
    │
    ├── configs/                 # Test-specific config sets
    │   ├── default/             # Standard config (init, keys, menu, style, apps)
    │   ├── click_focus/         # ClickToFocus config variant
    │   ├── sloppy_focus/        # SloppyFocus config variant
    │   ├── hc_dark/             # High-contrast dark theme
    │   ├── hc_light/            # High-contrast light theme
    │   ├── custom_rules/        # Window rules for apps-file testing
    │   └── minimal/             # Minimal config for fast tests
    │
    ├── reference/               # Reference screenshots (golden images)
    │   ├── desktop_empty.png
    │   ├── window_focused.png
    │   ├── window_unfocused.png
    │   ├── window_maximized.png
    │   ├── menu_root.png
    │   ├── toolbar_default.png
    │   └── ...
    │
    ├── results/                 # Test run output (gitignored)
    │   ├── screenshots/         # Actual screenshots from latest run
    │   ├── diffs/               # Visual diff images
    │   ├── report.html          # HTML test report
    │   └── report.json          # Machine-readable results
    │
    ├── test_windows.py          # TC-WIN-*: window operations
    ├── test_movement.py         # TC-MOVE-*: move/resize
    ├── test_layers.py           # TC-LAYER-*: window layers
    ├── test_workspaces.py       # TC-WS-*: workspace management
    ├── test_focus.py            # TC-FOCUS-*: focus models
    ├── test_decorations.py      # TC-DECOR-*: decoration rendering
    ├── test_tabs.py             # TC-TAB-*: tab groups
    ├── test_toolbar.py          # TC-TB-*: toolbar
    ├── test_slit.py             # TC-SLIT-*: slit/dock
    ├── test_menus.py            # TC-MENU-*: menu system
    ├── test_keybindings.py      # TC-KEY-*: key/mouse bindings
    ├── test_placement.py        # TC-PLACE-*: window placement
    ├── test_rules.py            # TC-RULE-*: window rules (apps file)
    ├── test_arrangement.py      # TC-ARR-*: window arrangement
    ├── test_styles.py           # TC-STYLE-*: theme/style switching
    ├── test_ipc.py              # TC-IPC-*: IPC protocol
    ├── test_config.py           # TC-CFG-*: configuration
    └── test_accessibility.py    # Accessibility features
```

---

## 4. Automation Library Design

### 4.1 IPC Client (`lib/ipc_client.py`)

The IPC client is the most critical component. Fluxland exposes a JSON-over-Unix-
socket protocol with 80+ commands, 7 query types, and 11 event subscriptions.

```python
class FluxlandIPC:
    """Client for fluxland's JSON IPC protocol."""

    def __init__(self, socket_path=None):
        """Connect to fluxland IPC socket.

        Auto-discovers socket at $XDG_RUNTIME_DIR/fluxland.$WAYLAND_DISPLAY.sock
        """

    # Query methods
    def ping(self) -> bool
    def get_windows(self) -> list[dict]
    def get_workspaces(self) -> list[dict]
    def get_outputs(self) -> list[dict]
    def get_config(self) -> dict

    # Command execution
    def command(self, action: str) -> dict
    def command_on(self, window_id: str, action: str) -> dict

    # Event subscription
    def subscribe(self, events: list[str]) -> EventStream
    def wait_for_event(self, event_type: str, timeout: float = 5.0) -> dict

    # Convenience wrappers
    def get_focused_window(self) -> dict | None
    def get_window_by_title(self, title: str) -> dict | None
    def get_window_by_app_id(self, app_id: str) -> dict | None
    def get_active_workspace(self) -> dict
```

**Usage in tests:**
```python
def test_maximize(ipc, test_window):
    """TC-WIN-03: Maximize toggles window to fill output."""
    original = ipc.get_focused_window()
    output = ipc.get_outputs()[0]

    ipc.command("Maximize")
    time.sleep(0.1)

    win = ipc.get_focused_window()
    assert win["maximized"] is True
    assert win["width"] == output["width"]
    assert win["height"] == output["height"] - TOOLBAR_HEIGHT

    # Toggle back
    ipc.command("Maximize")
    time.sleep(0.1)

    win = ipc.get_focused_window()
    assert win["maximized"] is False
    assert win["width"] == original["width"]
```

### 4.2 Input Driver (`lib/input_driver.py`)

Wraps `wtype` (keyboard) and `wlrctl` (pointer) for input simulation.

```python
class InputDriver:
    """Simulate keyboard and pointer input on Wayland."""

    # Keyboard (via wtype)
    def key(self, key: str, modifiers: list[str] = None)
    def type_text(self, text: str, delay_ms: int = 50)
    def key_down(self, key: str)
    def key_up(self, key: str)

    # Pointer (via wlrctl)
    def click(self, x: int, y: int, button: int = 1)
    def double_click(self, x: int, y: int, button: int = 1)
    def right_click(self, x: int, y: int)
    def move_to(self, x: int, y: int)
    def drag(self, x1: int, y1: int, x2: int, y2: int, button: int = 1)

    # High-level helpers
    def click_window_button(self, window: dict, button: str)  # "close", "maximize", etc.
    def click_titlebar(self, window: dict)
    def double_click_titlebar(self, window: dict)
    def drag_titlebar(self, window: dict, dx: int, dy: int)
    def right_click_desktop(self)
```

**Note:** `wlrctl` uses the wlr-virtual-pointer protocol. If unavailable, fall
back to IPC commands (most operations don't need real pointer input).

### 4.3 Screenshot Engine (`lib/screenshot.py`)

```python
class ScreenshotEngine:
    """Capture and compare screenshots using grim + Pillow."""

    def __init__(self, reference_dir: str, results_dir: str,
                 tolerance_percent: float = 2.0,
                 channel_threshold: int = 5):
        """Initialize with paths and comparison tolerances."""

    def capture(self, name: str, region: tuple = None,
                output: str = None) -> Path:
        """Capture screenshot via grim. Returns path to PNG."""

    def capture_region(self, x: int, y: int, w: int, h: int,
                       name: str) -> Path:
        """Capture a specific screen region."""

    def compare(self, actual: Path, reference: Path) -> CompareResult:
        """Compare two images pixel-by-pixel with tolerance."""

    def assert_matches_reference(self, name: str, region: tuple = None):
        """Capture screenshot and compare against reference image.

        On first run (no reference exists), saves as new reference (bootstrap).
        On subsequent runs, compares and fails if difference exceeds tolerance.
        Saves diff image to results/diffs/ on failure.
        """

    def assert_region_color(self, x: int, y: int, w: int, h: int,
                            expected_color: tuple, tolerance: int = 10):
        """Assert a screen region is approximately a solid color."""

    def assert_region_not_empty(self, x: int, y: int, w: int, h: int):
        """Assert a screen region is not blank/transparent."""


@dataclass
class CompareResult:
    match: bool
    diff_percent: float          # % of pixels that differ
    max_channel_diff: int        # Largest single-channel difference
    diff_image_path: Path | None # Path to visual diff (red highlights)
    actual_path: Path
    reference_path: Path
```

**Comparison Strategy:**
- Default tolerance: 2% pixel difference, 5 per-channel threshold
- Handles anti-aliasing artifacts and minor font rendering differences
- Diff images highlight mismatched pixels in red for easy visual inspection
- Bootstrap mode: first run with `--bootstrap` flag saves actuals as references
- Region-based comparison for testing specific UI elements (titlebar, toolbar, menu)

### 4.4 Compositor Manager (`lib/compositor.py`)

```python
class CompositorManager:
    """Manage fluxland compositor lifecycle for testing."""

    def __init__(self, binary_path: str = "fluxland",
                 config_dir: str = None):
        """Configure compositor launch parameters."""

    def start(self, config_set: str = "default",
              headless: bool = False,
              ipc_no_exec: bool = False) -> subprocess.Popen:
        """Start fluxland with the given config set.

        Live mode: starts on current WAYLAND_DISPLAY
        Headless mode: WLR_BACKENDS=headless WLR_RENDERER=pixman
        """

    def stop(self, timeout: float = 5.0):
        """Gracefully stop compositor via IPC Exit command."""

    def restart(self, config_set: str = None):
        """Stop and restart with optionally different config."""

    def reconfigure(self):
        """Send Reconfigure command via IPC."""

    def set_config(self, config_set: str):
        """Copy config files from configs/<set>/ to runtime config dir."""

    def wait_ready(self, timeout: float = 5.0):
        """Wait for IPC socket to appear and respond to ping."""
```

### 4.5 Test Window Factory (`lib/wayland_client.py`)

Creates lightweight Wayland client windows for testing, without needing real
applications.

```python
class TestWindowFactory:
    """Create test Wayland surfaces for UI testing."""

    def create_window(self, title: str = "Test Window",
                      app_id: str = "fluxland-test",
                      width: int = 640, height: int = 480,
                      color: tuple = (0.2, 0.3, 0.8, 1.0)) -> TestWindow:
        """Create an xdg_toplevel surface with solid color content."""

    def create_windows(self, count: int, **kwargs) -> list[TestWindow]:
        """Create multiple test windows."""

    def close_all(self):
        """Close all test windows created by this factory."""


class TestWindow:
    """A minimal Wayland test surface."""
    title: str
    app_id: str
    width: int
    height: int

    def close(self)
    def set_title(self, title: str)
    def resize(self, width: int, height: int)
```

**Implementation options:**
1. Python Wayland client via `pywayland` (if installable)
2. Small C helper binary compiled alongside the test suite (reuse integration
   harness patterns)
3. Launch `foot` (terminal) or `weston-simple-egl` as proxy test windows

Recommended: **Option 3 for initial implementation** (launch real apps like `foot`
via IPC `Exec foot --title "Test Window 1"`), progressing to Option 2 for
deterministic control.

---

## 5. Test Case Structure

### Standard Test Pattern

Every test follows this lifecycle:

```
┌─────────┐    ┌─────────┐    ┌─────────┐    ┌─────────┐    ┌─────────┐
│  Setup  │───▶│ Action  │───▶│  Wait   │───▶│ Assert  │───▶│ Cleanup │
│         │    │         │    │         │    │         │    │         │
│ - Config│    │ - IPC   │    │ - Event │    │ - State │    │ - Close │
│ - Windows│   │ - Input │    │ - Timer │    │ - Visual│    │ - Reset │
│ - State │    │ - Both  │    │         │    │ - Both  │    │         │
└─────────┘    └─────────┘    └─────────┘    └─────────┘    └─────────┘
```

### pytest Fixtures

```python
# conftest.py

@pytest.fixture(scope="session")
def compositor():
    """Session-scoped compositor instance."""
    mgr = CompositorManager()
    mgr.start(config_set="default")
    yield mgr
    mgr.stop()

@pytest.fixture
def ipc(compositor):
    """Per-test IPC client."""
    client = FluxlandIPC()
    yield client
    client.close()

@pytest.fixture
def screenshot(compositor):
    """Per-test screenshot engine."""
    return ScreenshotEngine(
        reference_dir="tests/ui/reference",
        results_dir="tests/ui/results"
    )

@pytest.fixture
def input_drv():
    """Per-test input driver."""
    return InputDriver()

@pytest.fixture
def test_windows(ipc):
    """Per-test window factory with automatic cleanup."""
    factory = TestWindowFactory(ipc)
    yield factory
    factory.close_all()
    time.sleep(0.2)  # Wait for windows to close

@pytest.fixture(autouse=True)
def clean_state(ipc):
    """Reset compositor state between tests."""
    yield
    # Post-test cleanup
    ipc.command("HideMenus")
    ipc.command("CloseAllWindows")
    time.sleep(0.3)
    ipc.command("Workspace 0")
```

### Example Test File

```python
# test_windows.py
"""TC-WIN: Window management operations."""

import pytest
import time


class TestWindowBasics:
    """Basic window lifecycle operations."""

    def test_close(self, ipc, test_windows):
        """TC-WIN-01: Close removes window from window list."""
        test_windows.create_window(title="CloseMe")
        time.sleep(0.3)

        windows = ipc.get_windows()
        assert any(w["title"] == "CloseMe" for w in windows)

        ipc.command("Close")
        time.sleep(0.3)

        windows = ipc.get_windows()
        assert not any(w["title"] == "CloseMe" for w in windows)

    def test_maximize_toggle(self, ipc, test_windows):
        """TC-WIN-03: Maximize fills output, toggles back."""
        test_windows.create_window(title="MaxMe")
        time.sleep(0.3)

        original = ipc.get_focused_window()
        orig_w, orig_h = original["width"], original["height"]
        output = ipc.get_outputs()[0]

        ipc.command("Maximize")
        time.sleep(0.2)

        win = ipc.get_focused_window()
        assert win["maximized"] is True
        # Width should fill output (accounting for borders)
        assert win["width"] >= output["width"] - 10

        ipc.command("Maximize")
        time.sleep(0.2)

        win = ipc.get_focused_window()
        assert win["maximized"] is False
        assert win["width"] == orig_w

    def test_fullscreen(self, ipc, test_windows, screenshot):
        """TC-WIN-04: Fullscreen fills entire output, hides decorations."""
        test_windows.create_window(title="FullscreenMe")
        time.sleep(0.3)

        ipc.command("Fullscreen")
        time.sleep(0.3)

        win = ipc.get_focused_window()
        output = ipc.get_outputs()[0]
        assert win["fullscreen"] is True
        assert win["width"] == output["width"]
        assert win["height"] == output["height"]

        # Visual: no decorations visible
        screenshot.assert_matches_reference("window_fullscreen")


class TestWindowVisual:
    """Visual regression tests for window appearance."""

    def test_focused_unfocused_decoration(self, ipc, test_windows, screenshot):
        """TC-DECOR-01: Focused vs unfocused window decorations differ."""
        test_windows.create_window(title="Window A")
        time.sleep(0.2)
        test_windows.create_window(title="Window B")
        time.sleep(0.3)

        # Window B is focused, A is unfocused
        screenshot.assert_matches_reference("two_windows_focus_state")
```

---

## 6. Screenshot Comparison Strategy

### Reference Image Management

```
reference/
├── default/                  # Default style reference images
│   ├── 1920x1080/           # Resolution-specific
│   │   ├── desktop_empty.png
│   │   ├── window_single.png
│   │   └── ...
│   └── 1280x720/
│       └── ...
├── hc_dark/                  # High-contrast dark theme
│   └── 1920x1080/
│       └── ...
└── hc_light/                 # High-contrast light theme
    └── 1920x1080/
        └── ...
```

### Bootstrap Workflow

```bash
# First run: capture reference images (human reviews them manually)
cd tests/ui
python -m pytest --bootstrap-screenshots -v

# Subsequent runs: compare against references
python -m pytest -v

# Update a specific reference after intentional visual change
python -m pytest test_toolbar.py::test_toolbar_visible --update-reference
```

### Comparison Tolerances

| Element | Tolerance | Rationale |
|---------|-----------|-----------|
| Window decorations | 2% pixels, ±5 channel | Font rendering variance |
| Solid color regions | 0.1% pixels, ±2 channel | Should be near-exact |
| Menu text | 3% pixels, ±5 channel | Text anti-aliasing |
| Full desktop | 5% pixels, ±8 channel | Multiple variance sources |
| Toolbar clock | Skip region | Time changes between runs |

### Dynamic Region Masking

Some screen regions change between runs (clock, timestamps). The comparison
engine supports rectangular masks:

```python
screenshot.assert_matches_reference(
    "toolbar_default",
    masks=[
        Region(x=clock_x, y=0, w=clock_w, h=toolbar_h),  # Mask the clock
    ]
)
```

---

## 7. Bug Reporting & Test Results

### Report Format

Each test run produces:

1. **`results/report.json`** — Machine-readable results
2. **`results/report.html`** — Human-readable HTML report with embedded images
3. **`results/screenshots/`** — All screenshots from the run
4. **`results/diffs/`** — Visual diff images for failed comparisons

### HTML Report Structure

```
┌─────────────────────────────────────────────────┐
│  Fluxland UI Test Report — 2026-02-27 15:30     │
│  ✅ 87 passed  ❌ 5 failed  ⚠️ 3 skipped        │
├─────────────────────────────────────────────────┤
│                                                  │
│  ❌ TC-DECOR-01: Focused decoration colors       │
│  ┌────────────┐ ┌────────────┐ ┌────────────┐  │
│  │ Reference  │ │  Actual    │ │   Diff     │  │
│  │  (golden)  │ │ (captured) │ │ (red=diff) │  │
│  └────────────┘ └────────────┘ └────────────┘  │
│                                                  │
│  Diff: 4.2% pixels differ (threshold: 2.0%)     │
│  Max channel diff: 42 (threshold: 5)            │
│                                                  │
│  Bug Notes:                                      │
│  - Unfocused titlebar gradient appears to use    │
│    wrong end color (#3a3a3a instead of #2a2a2a) │
│  - Compare with Fluxbox behavior: unfocused      │
│    titlebars should use window.label.unfocus.*   │
│                                                  │
│  IPC State at failure:                           │
│  {                                               │
│    "windows": [...],                             │
│    "focused": "Window A",                        │
│    "workspace": 0                                │
│  }                                               │
│                                                  │
│  Steps to Reproduce:                             │
│  1. Open two windows                             │
│  2. Focus Window B                               │
│  3. Compare Window A decoration colors           │
│                                                  │
├─────────────────────────────────────────────────┤
│  ✅ TC-WIN-03: Maximize toggle                   │
│  ... (collapsed for passing tests)               │
└─────────────────────────────────────────────────┘
```

### Failure Metadata

Every failure captures:

```python
@dataclass
class TestFailure:
    test_id: str                    # "TC-WIN-03"
    test_name: str                  # "test_maximize_toggle"
    category: str                   # "Window Management"
    severity: str                   # "critical" | "major" | "minor" | "cosmetic"
    description: str                # What went wrong
    expected_behavior: str          # What Fluxbox does
    actual_behavior: str            # What fluxland did
    steps_to_reproduce: list[str]   # Ordered steps
    screenshot_actual: Path | None
    screenshot_reference: Path | None
    screenshot_diff: Path | None
    ipc_state: dict                 # Compositor state at failure time
    compositor_log: str             # Relevant log lines
    timestamp: str
```

---

## 8. Feature Coverage Map

### Test File → Feature Area Mapping

| Test File | Feature Area | Test Cases | Priority |
|-----------|-------------|------------|----------|
| `test_windows.py` | Window operations | TC-WIN-01 through TC-WIN-08 | High |
| `test_movement.py` | Move/resize | TC-MOVE-01 through TC-MOVE-04 | High |
| `test_layers.py` | Window layers | TC-LAYER-01 through TC-LAYER-04 | Medium |
| `test_workspaces.py` | Workspaces | TC-WS-01 through TC-WS-09 | High |
| `test_focus.py` | Focus models | TC-FOCUS-01 through TC-FOCUS-04 | High |
| `test_decorations.py` | Decorations | TC-DECOR-01 through TC-DECOR-10 | High |
| `test_tabs.py` | Tab groups | TC-TAB-01 through TC-TAB-06 | Medium |
| `test_toolbar.py` | Toolbar | TC-TB-01 through TC-TB-09 | High |
| `test_slit.py` | Slit/dock | TC-SLIT-01 through TC-SLIT-03 | Low |
| `test_menus.py` | Menus | TC-MENU-01 through TC-MENU-10 | High |
| `test_keybindings.py` | Key/mouse bindings | TC-KEY-01 through TC-KEY-10 | Medium |
| `test_placement.py` | Window placement | TC-PLACE-01 through TC-PLACE-03 | Medium |
| `test_rules.py` | Window rules | TC-RULE-01 through TC-RULE-08 | Medium |
| `test_arrangement.py` | Arrangement | TC-ARR-01 through TC-ARR-05 | Medium |
| `test_styles.py` | Themes/styles | TC-STYLE-01 through TC-STYLE-05 | Medium |
| `test_ipc.py` | IPC protocol | TC-IPC-01 through TC-IPC-08 | High |
| `test_config.py` | Configuration | TC-CFG-01 through TC-CFG-04 | Medium |
| `test_accessibility.py` | Accessibility | Focus nav, borders, high contrast | Medium |

**Total: ~108 test cases across 18 test files**

---

## 9. Repeatability & State Management

### Pre-test State Reset

Each test gets a clean slate via the `clean_state` fixture:

1. Hide all menus (`HideMenus`)
2. Close all test windows (`CloseAllWindows`)
3. Switch to workspace 0
4. Wait for settle (300ms)

### Config Isolation

Tests that need specific configs use a config-set fixture:

```python
@pytest.fixture
def click_focus_config(compositor):
    """Switch to click-to-focus config for this test."""
    compositor.set_config("click_focus")
    compositor.reconfigure()
    yield
    compositor.set_config("default")
    compositor.reconfigure()
```

### Display Resolution Handling

- Reference images are resolution-specific (`reference/<style>/<WxH>/`)
- Test runner detects current resolution via `get_outputs` IPC
- Falls back to closest resolution references if exact match unavailable
- Resolution recorded in test results for reproducibility

### Deterministic Window Positioning

For visual tests, windows are placed at exact coordinates:

```python
def test_decoration_visual(ipc, test_windows, screenshot):
    test_windows.create_window(title="Test")
    time.sleep(0.3)
    ipc.command("MoveTo 100 100")
    ipc.command("ResizeTo 640 480")
    time.sleep(0.2)
    screenshot.assert_matches_reference("decoration_focused")
```

---

## 10. Running the Test Suite

### Quick Start

```bash
# Install Python dependencies
cd tests/ui
pip install -r requirements.txt

# Install system tools
sudo apt install wlrctl slurp imagemagick

# First run: bootstrap reference screenshots
# (must be running in a fluxland Wayland session)
python -m pytest --bootstrap-screenshots -v 2>&1 | tee results/bootstrap.log

# Review reference images manually in tests/ui/reference/

# Normal test run
python -m pytest -v 2>&1 | tee results/test.log

# Run specific test category
python -m pytest test_windows.py -v

# Run with HTML report
python -m pytest --html=results/report.html -v

# Run headless (no visual tests, state-only assertions)
FLUXLAND_TEST_HEADLESS=1 python -m pytest -v -m "not visual"
```

### pytest Markers

```python
# In each test file, mark visual tests:
@pytest.mark.visual
def test_decoration_colors(screenshot, ...):
    """This test requires a real display for screenshots."""

@pytest.mark.slow
def test_all_arrangement_modes(...):
    """This test takes >10 seconds."""

@pytest.mark.requires_wlrctl
def test_mouse_drag_titlebar(input_drv, ...):
    """This test requires wlrctl for pointer simulation."""
```

```bash
# Skip visual tests (headless CI)
python -m pytest -m "not visual"

# Only visual tests
python -m pytest -m visual

# Skip slow tests
python -m pytest -m "not slow"
```

### CI Integration (GitHub Actions)

```yaml
# .github/workflows/ui-tests.yml
name: UI Tests (Headless)
on: [push, pull_request]
jobs:
  ui-test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y meson ninja-build libwlroots-dev \
            libwayland-dev libxkbcommon-dev libpango1.0-dev \
            python3-pytest python3-pil
      - name: Build fluxland
        run: |
          meson setup build
          ninja -C build
      - name: Run headless UI tests
        run: |
          cd tests/ui
          FLUXLAND_TEST_HEADLESS=1 python -m pytest -m "not visual" -v
```

For **full visual tests** in CI, use a Wayland virtual framebuffer:

```yaml
      - name: Run visual tests in cage
        run: |
          sudo apt-get install -y cage grim wtype wlrctl
          export WLR_BACKENDS=headless WLR_RENDERER=pixman
          cage -- bash -c "
            sleep 1
            cd tests/ui
            python -m pytest -m visual -v
          "
```

---

## 11. Implementation Phases

### Phase 1: Foundation (Week 1)
- [ ] Create `tests/ui/` directory structure
- [ ] Implement `lib/ipc_client.py` — IPC JSON client
- [ ] Implement `lib/screenshot.py` — grim wrapper + Pillow comparison
- [ ] Implement `lib/compositor.py` — basic lifecycle management
- [ ] Write `conftest.py` with core fixtures
- [ ] Install `wlrctl` on VM
- [ ] Write 5 smoke tests: ping, get_windows, get_workspaces, basic window ops

### Phase 2: Core Window Tests (Week 2)
- [ ] Implement `lib/input_driver.py` — wtype/wlrctl wrapper
- [ ] Write `test_windows.py` — all TC-WIN tests (8 tests)
- [ ] Write `test_movement.py` — all TC-MOVE tests (4 tests)
- [ ] Write `test_workspaces.py` — all TC-WS tests (9 tests)
- [ ] Write `test_ipc.py` — all TC-IPC tests (8 tests)
- [ ] Bootstrap reference screenshots for these tests

### Phase 3: UI Element Tests (Week 3)
- [ ] Write `test_decorations.py` — all TC-DECOR tests (10 tests)
- [ ] Write `test_toolbar.py` — all TC-TB tests (9 tests)
- [ ] Write `test_menus.py` — all TC-MENU tests (10 tests)
- [ ] Write `test_focus.py` — all TC-FOCUS tests (4 tests)
- [ ] Implement dynamic region masking for clock

### Phase 4: Advanced Features (Week 4)
- [ ] Write `test_tabs.py` — TC-TAB tests (6 tests)
- [ ] Write `test_keybindings.py` — TC-KEY tests (10 tests)
- [ ] Write `test_rules.py` — TC-RULE tests (8 tests)
- [ ] Write `test_arrangement.py` — TC-ARR tests (5 tests)
- [ ] Write `test_placement.py` — TC-PLACE tests (3 tests)
- [ ] Write `test_styles.py` — TC-STYLE tests (5 tests)
- [ ] Write `test_layers.py` — TC-LAYER tests (4 tests)
- [ ] Write `test_slit.py` — TC-SLIT tests (3 tests)

### Phase 5: Reporting & Polish (Week 5)
- [ ] Implement `lib/report.py` — HTML report generator
- [ ] Write `test_config.py` — TC-CFG tests (4 tests)
- [ ] Write `test_accessibility.py`
- [ ] Create config variants (click_focus, sloppy_focus, hc_dark, hc_light)
- [ ] Full reference screenshot bootstrap pass
- [ ] Document the harness in README
- [ ] CI pipeline setup

---

## 12. Key Design Decisions

### Why Python (not C or shell scripts)?

- **pytest** provides excellent test discovery, fixtures, markers, and reporting
- **Pillow** handles image comparison without external dependencies
- Rich standard library for JSON, sockets, subprocess management
- Easy to extend and maintain
- Shell scripts lack structured assertion and reporting
- C would duplicate effort and is harder to iterate on

### Why IPC-first (not input simulation)?

- IPC commands are **deterministic** — no timing issues with key events
- IPC provides **state queries** — can verify results without screenshots
- 80+ commands cover nearly all compositor operations
- Input simulation (wtype/wlrctl) used only where IPC can't reach:
  mouse interaction testing, keyboard navigation, titlebar button clicks

### Why grim + Pillow (not the existing C image_compare)?

- The C image_compare library works at the render-API level (unit tests)
- We need **compositor-level screenshots** of the actual desktop
- grim captures what the user sees (the complete composited output)
- Pillow is more flexible for masking, cropping, and report generation
- The C library's approach (tolerance %, channel threshold) is replicated in Python

### Why region-based comparison?

- Full-desktop screenshots are fragile (any change anywhere fails the test)
- Region comparison isolates the element under test (titlebar, menu, toolbar)
- Enables **parallel element testing** in a single screenshot
- Clock/time regions can be masked without affecting other assertions

---

## 13. Fluxbox Behavior Reference

The test suite validates fluxland behavior against upstream Fluxbox conventions.
Key behavioral expectations are documented inline in each test file as docstrings:

```python
def test_shade(ipc, test_windows):
    """TC-WIN-06: Shade collapses window to titlebar only.

    Fluxbox behavior: Window shading reduces the window to just its
    titlebar. The window content is hidden but the titlebar remains
    visible and interactive. Shading again restores the full window.
    The shaded window should retain its width and x,y position.
    """
```

For comprehensive Fluxbox behavior documentation, see:
- [Fluxbox Wiki](http://fluxbox-wiki.org/)
- [Fluxbox man pages](https://fluxbox.org/help/man-fluxbox.php)
- The feature catalog in this project's planning documents

---

## 14. Known Limitations & Mitigations

| Limitation | Mitigation |
|------------|------------|
| No mouse injection without wlrctl | Install wlrctl; fall back to IPC for most operations |
| Clock changes between runs | Mask clock region in toolbar screenshots |
| Font rendering varies by system | Per-channel tolerance (±5); resolution-specific references |
| Animations cause timing issues | Add settle delays after actions; disable animations in test config |
| Headless mode has no visual output | Use `cage` for visual tests in CI; mark visual tests separately |
| Window creation is async | Wait for IPC window_open event or poll get_windows |
| XWayland has known issues on VM | Skip XWayland-specific tests; mark with `@pytest.mark.xwayland` |

---

## 15. Future Extensions

- **Video recording**: Use `wf-recorder` to capture full test sessions as MP4
  for debugging failures
- **Performance benchmarks**: Measure frame time during window operations via
  IPC timing
- **Accessibility audit**: Automated WCAG contrast ratio checking on screenshots
- **Multi-monitor testing**: Use `wlr-randr` to simulate multi-output setups
- **Stress testing**: Create/destroy hundreds of windows, rapid workspace switching
- **Fuzzing integration**: Feed fuzz-generated configs and test for crashes
- **Fluxbox comparison**: Side-by-side screenshots of Fluxbox (X11) vs fluxland
  (Wayland) for the same operations
