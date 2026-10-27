# Fluxland

Fluxbox-inspired Wayland compositor built on wlroots 0.20 (pinned subproject, `subprojects/wlroots.wrap`; a system wlroots-0.20 is used if present; libraries newer than the distribution's come from `subprojects/` too, all static). C17, Meson, MIT license. Version 2.0.0.

## Build & Test

```sh
meson setup build
ninja -C build
meson test -C build --print-errorlogs --suite fluxland   # C unit + integration tests (not the bundled libraries' own)
python3 -m pytest tests/ui/ --timeout=30 # Python UI tests (headless: FLUXLAND_TEST_HEADLESS=1)
sudo meson install -C build --skip-subprojects
```

Build options: `-Dxwayland=enabled`, `-Dasan=true`, `-Dubsan=true`

## Project Layout

```
src/           116 files (59 .c + 57 .h), ~35K lines — compositor source
tests/         53 C test suites, 8 fuzz targets
tests/ui/      21 Python pytest files — 174 end-to-end UI tests via IPC
data/          Default configs (init, keys, apps, menu, style, startup) + styles
examples/      Example config overlays (wave-desktop, minimal, sloppy-focus) + styles
docs/          QUICKSTART.md, design/ARCHITECTURE.md, STYLES.md, SCREEN_RECORDING.md
man/           8 man pages (fluxland.1, fluxland-keys.5, etc.)
packaging/     PKGBUILD (Arch), fluxland.spec (Fedora), Flatpak, debian/
```

## Architecture

- **Scene graph rendering** via wlroots `wlr_scene` API (damage tracking, compositing)
- **Views have backends**: `view.c` holds window management shared by all windows; `view_xdg.c` (xdg-shell) and `xwayland.c` (X11) implement `struct wm_view_impl`. X11 windows are ordinary `wm_view`s (decorations, rules, tabs, iconbar, commands); override-redirect X11 windows are unmanaged and drawn above windows
- **Server-side decorations** as scene buffers (NOT wl_surfaces — affects pointer focus)
- **Text rendering** with Pango/Cairo: `wm_render_text()`, `wm_measure_text_width()`
- **Command language**: `command.c` parses and runs Fluxbox commands (MacroCmd, ToggleCmd, If/Cond, ForEach/Map, Delay, nested) for key bindings, mouse bindings, menu `[command]` items and IPC alike
- **Action dispatch**: one dispatcher, `wm_execute_action_on()` in `keyboard_actions.c` (110+ actions); `keyboard.c` only routes key events
- **Client patterns**: `client_pattern.c` implements Fluxbox's `(name=...) (title!=...) (workspace=[current]) {N}` matching (anchored POSIX EREs) for conditions and window lists
- **IPC**: JSON-over-Unix-socket with SO_PEERCRED auth, event subscriptions
- **Config**: Fluxbox-compatible parser in `rcparser.c`, validation in `validate.c`
- **Central state**: `server.h` holds 98 wlroots protocol interfaces in one struct

### Key modules by size

| Module | Lines | Role |
|--------|-------|------|
| menu.c | 2,920 | Menu system with submenus, key nav, RTL |
| view.c | 1,931 | Window lifecycle, focus, XDG toplevel |
| decoration.c | 1,821 | SSD titlebar, buttons, resize handles |
| cursor.c | 1,796 | Pointer input, move/resize, snap zones |
| ipc_commands.c | ~700 | IPC protocol (queries, subscriptions; commands go through command.c) |
| toolbar.c | 1,523 | Workspace switcher, iconbar, clock |
| keyboard_actions.c | 1,282 | 110+ WM action handlers |

## Critical Patterns

### Never touch xdg_toplevel/xsurface outside the backends
Use `wm_view_surface()`, `wm_view_set_size()`, `wm_view_content_geometry()`, `wm_view_close()`, `wm_view_set_activated()` etc. Code that reaches into `view->xdg_toplevel` silently breaks X11 windows. Unit tests without view.c get these from `tests/view_backend_stubs.c` (opt-in `VIEW_STUB_*` defines).

### Commands act on a target window
Window actions run against a target view: the clicked window for mouse bindings, each window for ForEach, the focused window otherwise. Handlers must use the `view` they're given, never `server->focused_view` directly. Add new actions to the single table in `keybind.c` and the dispatcher in `keyboard_actions.c`; keys, mouse, menus and IPC all get them.

### Focus order is `focus_seq`, not list order
`server->views` is stacking order. Most-recently-used order is `view->focus_seq`; NextWindow cycling freezes it while modifiers are held (`server->focus_cycle`) and commits on release.

### Child processes: always double-fork
All 5 exec paths (`keyboard.c`, `menu.c`, `cursor.c`, `autostart.c`, `ipc_commands.c`) use double-fork to avoid SIGCHLD racing with wlroots. Never use `waitpid(-1)` in a compositor.

### Pango text measurement
Use `pango_layout_get_pixel_extents()` ink rect. NEVER use `pango_layout_get_pixel_size()` — it underestimates rendered width.

### Decorations are scene buffers
Decorations are NOT `wl_surfaces`, so pointer focus is NULL when clicking titlebars. Code that checks `focused_surface` must handle NULL.

### Decoration geometry refresh
`content_width`/`content_height` must be refreshed on client commit via `wm_decoration_refresh_geometry()`, not just at map time.

### Tiled window hints
`wlr_xdg_toplevel_set_tiled()` is required for terminals to fill exact size (prevents pixel gaps in tiling layouts).

### Window rules
Setting a state flag (e.g. `view->maximized = true`) is NOT enough — must also apply physical geometry change.

### text_input_v3
Always check `wl_resource_get_client()` matches before `wlr_text_input_v3_send_enter()`. Virtual keyboards (wtype) create text_input resources from a different client.

### Iconbar updates
Call `wm_toolbar_update_iconbar()` in BOTH `unmap` and `destroy` handlers.

## Test Patterns

- Tests use `#include "source.c"` with header guards + local stubs
- When adding new public API, add stubs to ALL test files that `#include` that module
- Fuzz targets (8) use libFuzzer, require Clang: `fuzz_rcparser`, `fuzz_keybind`, `fuzz_style`, `fuzz_menu`, `fuzz_ipc_command`, `fuzz_rules`, `fuzz_mousebind`, `fuzz_command`
- UI tests connect via `FLUXLAND_SOCK` env var; support headless and live session modes
- Tests needing `/tmp/fluxland-test/` fail under sandbox
- UI harness logs compositor stderr to a file (an unread pipe fills and blocks the compositor); `FLUXLAND_TEST_KEEP_LOG=<path>` keeps it, `FLUXLAND_BINARY` picks the binary
- CI: `.github/workflows/ci.yml` (GitHub Actions; not run on Forgejo) runs `tools/ci.sh build|sanitize|fuzz|ui|wlcs` in `debian:trixie`; run the same steps locally
- WLCS: `tools/ci.sh wlcs` runs the conformance suite through `src/wlcs_shim.c`; `tests/wlcs/known-failures` lists the expected failures with reasons, anything else fails

## Security

- `safe_atoi()` in `util.h` — no raw `atoi()`/`atol()`
- `fopen_nofollow()` in `util.h` — prevents symlink attacks on config files
- SO_PEERCRED on IPC socket for client authentication
- Environment variable denylist for child processes
- Compiler hardening: `-fstack-protector-strong`, `-D_FORTIFY_SOURCE=2`, full RELRO

## Conventions

- C17, tabs for indentation, K&R braces
- Conventional commits: `feat:`, `fix:`, `docs:`, `test:`, `chore:`, `refactor:`
- All source files have `SPDX-License-Identifier: MIT` headers
- `packaging/debian/` is in `.gitignore` — use `git add -f` to stage changes there

## Config Files

User config at `~/.config/fluxland/` with: `init` (settings), `keys` (bindings), `apps` (window rules), `menu` (root menu), `style`, `startup` (autostart). Live reload via `Mod4+r` or `SIGHUP`.

## IPC

`command` requests take an optional `"window": <id>` to act on that window (`fluxland-ctl -w <id> action ...`).

Socket at `$XDG_RUNTIME_DIR/fluxland.$WAYLAND_DISPLAY.sock`. CLI tool: `fluxland-ctl`. Commands: `ping`, `get_windows`, `get_workspaces`, `get_outputs`, `get_config`, `command`, `subscribe`.
