# Fluxland

Fluxbox-inspired Wayland compositor built on wlroots 0.18. C17, Meson, MIT license. Version 1.0.0.

## Build & Test

```sh
meson setup build
ninja -C build
meson test -C build --print-errorlogs   # 53 C unit tests
python3 -m pytest tests/ui/ --timeout=30 # 154 Python UI tests
sudo ninja -C build install
```

Build options: `-Dxwayland=enabled`, `-Dasan=true`, `-Dubsan=true`

## Project Layout

```
src/           114 files (58 .c + 56 .h), ~35K lines — compositor source
tests/         52 C test files, 7 fuzz targets — 81.2% line coverage
tests/ui/      20 Python pytest files — 154 end-to-end UI tests via IPC
data/          Default configs (init, keys, apps, menu, style, startup) + themes
examples/      Complete example configs (wave-desktop, minimal, sloppy-focus)
docs/          QUICKSTART.md, design/ARCHITECTURE.md, design/FLUXBOX-COMPAT.md
man/           5 man pages (fluxland.1, fluxland-keys.5, etc.)
packaging/     PKGBUILD (Arch), fluxland.spec (Fedora), Flatpak, debian/
```

## Architecture

- **Scene graph rendering** via wlroots `wlr_scene` API (damage tracking, compositing)
- **Server-side decorations** as scene buffers (NOT wl_surfaces — affects pointer focus)
- **Text rendering** with Pango/Cairo: `wm_render_text()`, `wm_measure_text_width()`
- **Action dispatch** split: `keyboard.c` (event routing) + `keyboard_actions.c` (110+ actions)
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
| ipc_commands.c | 1,664 | IPC command handlers |
| toolbar.c | 1,523 | Workspace switcher, iconbar, clock |
| keyboard_actions.c | 1,282 | 110+ WM action handlers |

## Critical Patterns

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
- Fuzz targets (7) use libFuzzer, require Clang: `fuzz_rcparser`, `fuzz_keybind`, `fuzz_style`, `fuzz_menu`, `fuzz_ipc_command`, `fuzz_rules`, `fuzz_mousebind`
- UI tests connect via `FLUXLAND_SOCK` env var; support headless and live session modes
- Tests needing `/tmp/fluxland-test/` fail under sandbox

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

User config at `~/.config/fluxland/` with: `init` (settings), `keys` (bindings), `apps` (window rules), `menu` (root menu), `style` (theme), `startup` (autostart). Live reload via `Mod4+r` or `SIGHUP`.

## IPC

Socket at `$XDG_RUNTIME_DIR/fluxland.$WAYLAND_DISPLAY.sock`. CLI tool: `fluxland-ctl`. Commands: `ping`, `get_windows`, `get_workspaces`, `get_outputs`, `get_config`, `command`, `subscribe`.
