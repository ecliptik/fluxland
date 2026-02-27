# Fluxland UI Test Harness

Automated UI/UX testing for the fluxland Wayland compositor. Uses IPC commands
to drive the compositor in headless mode and verify behavior.

## Prerequisites

- Python 3.10+
- pytest
- A built fluxland binary (in `build/` or on PATH)
- `foot` terminal emulator (for test windows)

## Quick Start

```bash
cd tests/ui
python3 -m pytest -v
```

The harness automatically starts a headless compositor instance for the test session.

## Test Files

| File | Tests | Description |
|------|-------|-------------|
| test_smoke.py | 8 | Basic IPC connectivity and queries |
| test_ipc.py | 12 | IPC protocol coverage (commands, events, errors) |
| test_windows.py | 11 | Window operations (maximize, fullscreen, shade, etc.) |
| test_movement.py | 8 | Window move and resize |
| test_workspaces.py | 10 | Workspace management |
| test_focus.py | 4 | Focus cycling |
| test_decorations.py | 10 | Decoration toggle and state |
| test_toolbar.py | 9 | Toolbar visibility and focus navigation |
| test_menus.py | 10 | Menu open/close and events |
| test_tabs.py | 6 | Tab group operations |
| test_keybindings.py | 10 | Key binding management |
| test_rules.py | 8 | Window rules (Remember command) |
| test_arrangement.py | 5 | Window arrangement (grid, columns, rows, cascade) |
| test_placement.py | 3 | Window placement policy |
| test_styles.py | 5 | Style/theme loading |
| test_layers.py | 4 | Window layer operations |
| test_slit.py | 3 | Slit/dock operations |
| test_config.py | 4 | Configuration query and reload |
| test_accessibility.py | 6 | Accessibility features and events |

**Total: ~130 tests**

## Architecture

The harness runs in **headless mode** by default:
- Compositor starts with `WLR_BACKENDS=headless WLR_RENDERER=pixman`
- All interaction is via the IPC JSON protocol (Unix socket)
- Test windows are `foot` terminal instances launched via `Exec` IPC command
- Session-scoped compositor (one instance per test run)
- Per-test cleanup resets state (close windows, hide menus, switch to workspace 0)

### Library Components

| Module | Purpose |
|--------|---------|
| `lib/ipc_client.py` | IPC JSON client (commands, queries, event subscriptions) |
| `lib/compositor.py` | Compositor lifecycle (start/stop/config management) |
| `lib/input_driver.py` | Keyboard/pointer simulation (wtype/wlrctl) |
| `lib/screenshot.py` | Screenshot capture and comparison (grim/Pillow) |
| `lib/report.py` | HTML/JSON test report generation |

### Config Variants

Test configs live in `configs/`:
- `default/` — Standard config (ClickToFocus, RowSmartPlacement)
- `click_focus/` — Explicit click-to-focus
- `sloppy_focus/` — Mouse/sloppy focus model
- `hc_dark/` — High-contrast dark theme
- `hc_light/` — High-contrast light theme

## Adding Tests

1. Create a test file `test_<feature>.py`
2. Use the `ipc` fixture for IPC commands and queries
3. Use the `windows` fixture to open test windows (auto-cleanup)
4. Add `SETTLE = 0.2` and use `time.sleep(SETTLE)` after IPC commands
5. Follow existing patterns — tests should be independent and idempotent

## Environment Variables

- `FLUXLAND_TEST_HEADLESS=1` — Force headless mode
- `WAYLAND_DISPLAY` — If set and not headless, connects to live session
