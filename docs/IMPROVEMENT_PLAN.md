# Fluxland Improvement Plan

*Generated from comprehensive codebase review — 2026-03-02*
*Last updated: 2026-03-02 — 23 items completed across five sprints*

## Executive Summary

Fluxland is in strong shape for a 1.0.0 release. The 35K-line codebase demonstrates consistent coding conventions, thoughtful architecture (scene graph rendering, double-fork exec, server-side decorations), and solid security fundamentals (safe_atoi, fopen_nofollow, SO_PEERCRED, full RELRO, stack protector). The 38 C unit tests and 137 Python UI tests provide 81.2% line coverage — excellent for a compositor of this complexity.

The primary areas for improvement fall into three themes: **test coverage breadth** (22 of 47 source modules lack dedicated test files, with several large modules like `view.c`, `decoration.c`, and `cursor.c` untested), **function complexity** (several functions exceed 200–900 lines and would benefit from decomposition), and **missing infrastructure** (no CI/CD pipeline, incomplete i18n, missing man pages for `init` and `startup` configs). There are also several malloc calls without NULL checks and duplicated double-fork boilerplate across 5 files that could be consolidated.

The plan below is ordered by priority: critical fixes first, then high-ROI improvements, followed by longer-term enhancements. Each item includes affected files, effort estimates, and enough context to be immediately actionable.

---

## 1. Critical Fixes

Items that should be addressed before new feature work.

### 1.1 ~~Missing NULL checks after malloc/calloc~~ DONE
**Severity**: High | **Effort**: S | **Category**: Security/Robustness

Fixed in commit `eebb940`. Added NULL check in `strbuf_init()` (`ipc_commands.c`) — sets `cap = 0` on malloc failure. Full audit of all malloc/calloc/realloc calls in `src/` confirmed the other two cited locations (`toolbar.c:533`, `rules.c:429`) already had proper NULL checks.

### ~~1.2 Missing wlroots scene node error checks~~ DONE
**Severity**: Medium | **Effort**: S | **Category**: Robustness

Fixed in `7dc510c`. Added NULL checks with graceful cleanup in `wm_decoration_create()` (border rects, titlebar tree, scene buffers), `layout_and_render()` (button buffers), and `wm_toolbar_create()` (bg buffer).

### 1.3 ~~XWayland XKB keymap compilation failure~~ RESOLVED (environmental)
**Severity**: Medium | **Effort**: M | **Category**: Bug

Investigated in sprint 2. This is an environmental issue in the QEMU VM (missing xkeyboard-config data), not a code bug. The keyboard.c XKB path already has proper fallback logic. No code changes needed.

### ~~1.4 ShowDesktop may need 2 presses~~ DONE
**Severity**: Low | **Effort**: S | **Category**: Bug

Fixed in `f363998`. Root cause: `request_minimize.notify()` triggered focus changes that reordered the views list during `wl_list_for_each` iteration, causing skipped windows. Fix: directly set scene node disabled + foreign_toplevel minimized without triggering focus changes. Applied to both keyboard_actions.c and ipc_commands.c paths.

---

## 2. High-Value Improvements

Best ROI — biggest impact for effort spent.

### 2.1 Set up CI/CD pipeline
**Effort**: M | **Category**: Infrastructure

No CI/CD exists. The `.github/` directory has only a PR template — no workflows. The project has CLAUDE.md listing a Forgejo Actions need.

**Action**:
- Create `.github/workflows/build-test.yml` (or Forgejo equivalent) with: Meson build, `meson test` (C unit tests), `pytest tests/ui/` (if headless mode works in CI)
- Add compiler warning check (ensure `-Werror` in CI)
- Add ASAN/UBSAN build variant
- Consider `clang-format` check for style enforcement

### ~~2.2 Consolidate double-fork boilerplate into shared helper~~ DONE
**Effort**: S | **Category**: Code Quality

Fixed in `f363998`. Created `wm_spawn_command()` in new `src/util.c` with double-fork + setsid + closefrom + LD_* env sanitization. Replaced inline fork/exec in keyboard_actions.c, menu.c, cursor.c, ipc_commands.c (4 of 5 paths). autostart.c retained its callback-based pattern. Also deduplicated `is_blocked_env_var()` → `wm_is_blocked_env_var()` in util.h.

### ~~2.3 Decompose mega-functions~~ DONE
**Effort**: L | **Category**: Code Quality

Fixed in sprint 3. Decomposed 4 mega-functions (menu.c was already done in 3.3):

| Function | File | Before | After | Extracted |
|----------|------|--------|-------|-----------|
| `ipc_execute_action()` | `ipc_commands.c` | 960 lines | 65-line dispatcher | 9 grouped handlers (`handle_action_process`, `_window_state`, `_focus`, `_movement`, `_workspace`, `_tabs`, `_menus`, `_layout`, `_config`) |
| `wm_execute_action()` | `keyboard_actions.c` | 925 lines | clean dispatcher | ~24 action handlers (`handle_exec`, `handle_close`, `handle_maximize`, etc.) with trivial cases left inline |
| `layout_and_render()` | `decoration.c` | 590 lines | 115-line dispatcher | 5 render functions + `struct decoration_layout` (`render_decoration_borders`, `_titlebar`, `_handle`, `_ext_tabs`, `_focus_border`) |
| `apply_rc_to_config()` | `config.c` | 437 lines | 12-line dispatcher | 12 category helpers (`apply_workspace_config`, `_focus_config`, `_window_config`, `_toolbar_config`, `_iconbar_config`, `_tab_config`, `_slit_config`, `_menu_config`, `_xkb_config`, `_mouse_config`, `_misc_config`, `_file_paths_config`) |

Full QA pass (26/26 tests) confirmed no regressions. All 38 C unit tests pass.

### ~~2.4 Add environment variable sanitization to exec paths~~ DONE
**Effort**: S | **Category**: Security

Fixed in `f363998` (combined with 2.2). `wm_spawn_command()` strips LD_PRELOAD, LD_LIBRARY_PATH, LD_AUDIT, LD_DEBUG, LD_PROFILE before exec.

---

## 3. Code Quality & Technical Debt

### ~~3.1 IPC sprintf usage~~ DONE
**Effort**: S | **Category**: Security

Fixed in `6f1abc3`. Replaced `sprintf` with `snprintf` using proper remaining buffer size calculation.

### ~~3.2 Inconsistent error handling in config parsing~~ DONE
**Effort**: M | **Category**: Robustness

Fixed in sprint 2 (commit `876fe54`). Audited all config parsers:
- `mousebind.c`: Added long line handling to `fgets()` loop
- `rules.c`: Added long line handling to `fgets()` loop
- `slit.c`: Changed `fopen()` → `fopen_nofollow()` to prevent symlink attacks
- `rcparser.c`, `keybind.c`, `validate.c`: Already had proper error handling

### ~~3.3 Menu.c complexity~~ DONE
**Effort**: L | **Category**: Code Quality

Fixed in sprint 2 (commits `ccc2682`, `cc1c8b8`). Split menu.c into three modules:
- `menu_parse.c/h`: Config parsing, pipe menus, separator handling
- `menu_render.c/h`: Cairo/Pango rendering, RTL detection, layout constants
- `menu.c`: State management, input handling, interaction logic

Original 2,920 lines split into ~1,000 lines each.

### ~~3.4 View.c and cursor.c complexity~~ DONE
**Effort**: M per file | **Category**: Code Quality

Fixed in sprint 5. Split both files using the same internal-header pattern as menu.c:

**view.c** (1,932 → 629 lines):
- `view_focus.c/h` (~637 lines): Focus management, cycling, deiconify, layer stacking, directional focus
- `view_geometry.c/h` (~528 lines): Maximize, fullscreen, tiling halves, multi-monitor moves
- `wm_json_escape()` moved to `util.c/h` as shared utility
- Added `wm_view_toggle_maximize()` and `wm_view_toggle_fullscreen()` public API

**cursor.c** (1,796 → ~1,130 lines):
- `cursor_snap.c/h` (~340 lines): Snap zones, wireframes, position overlay, Cairo pixel buffer bridge
- `cursor_actions.c/h` (~320 lines): Mouse action dispatch, hit testing, context detection

Full QA: 49/49 C unit tests, 151 Python UI tests passed.

---

## 4. Test Coverage & Quality

### ~~4.1 Add test files for untested modules~~ DONE
**Effort**: XL (total) | **Category**: Testing

Fixed in sprint 4. Added 11 new test files (157 tests) covering all previously untested modules:

| Test file | Module(s) | Tests |
|-----------|-----------|-------|
| `test_input.c` | input.c | 17 — device mgmt, seat caps, drag handling |
| `test_text_input.c` | text_input.c | 18 — v3 lifecycle, client-match guard, IM relay |
| `test_tablet.c` | tablet.c | 17 — tool proximity, tip/button, pad input |
| `test_render.c` | render.c | 27 — color helpers, text rendering, ink extents |
| `test_rcparser.c` | rcparser.c | 21 — key-value parsing, int/bool coercion, file loading |
| `test_util.c` | util.c | 18 — safe_atoi, blocked env vars, fopen_nofollow |
| `test_screencopy.c` | screencopy.c | 4 — screencopy + DMA-BUF init |
| `test_drm_lease.c` | drm_lease.c | 8 — init, lease request, offer_output |
| `test_gamma_control.c` | gamma_control.c | 8 — init, set_gamma handler paths |
| `test_transient_seat.c` | transient_seat.c | 7 — init, seat creation, counter |
| `test_small_protocols.c` | fractional_scale, presentation, viewporter, drm_syncobj | 12 — init/failure paths |

Test count: 38 → 49 unit tests. Full QA: 150/150 UI tests passed.

### ~~4.2 Expand fuzz targets~~ DONE
**Effort**: M | **Category**: Testing

Fixed in sprint 4. Added 3 new fuzz targets + 14 seed corpus files:
- **fuzz_ipc_command** — JSON command parsing (network-facing attack surface)
- **fuzz_rules** — window rules/apps file parser
- **fuzz_mousebind** — mouse binding config parser

Fuzz target count: 4 → 7. Corpus: `tests/fuzz/corpus/{ipc_command,rules,mousebind}/`.

### ~~4.3 XWayland test coverage~~ DONE
**Effort**: L | **Category**: Testing

Expanded in sprint 2 (commit `ec5cf47`). Added 28 new XWayland tests to `test_xwayland_logic.c` (31→59 total), covering view lifecycle, geometry management, unmap/destroy handlers, and edge cases.

### ~~4.4 Integration test coverage~~ DONE
**Effort**: M | **Category**: Testing

Added in sprint 2 (commit `ec5cf47`). Created `tests/ui/test_edge_cases.py` with 16 integration edge case tests covering multi-monitor workspace switching, rapid window create/destroy cycles, IPC command injection attempts, and config live-reload during active window operations.

---

## 5. Documentation & Developer Experience

### ~~5.1 Missing man pages~~ DONE
**Effort**: M | **Category**: Documentation

Fixed in sprint 2 (commit `e70ad4f`). Created three new man pages:
- `fluxland-init.5` — all init file settings from `src/config.c`
- `fluxland-startup.5` — autostart script format and behavior
- `fluxland-ctl.1` — full IPC client reference with all commands, actions, events

Man page count: 5 → 8.

### ~~5.2 Internationalization infrastructure~~ DONE
**Effort**: M | **Category**: i18n

Fixed in sprint 2 (commits `876fe54`, `aaf8b21`). Added `_()` wrapping to `config.c`, `toolbar.c`, `keyboard_actions.c`. Updated `po/POTFILES.in`, created `po/meson.build` with gettext integration, added `po/LINGUAS`, regenerated `po/fluxland.pot` (40+ translatable strings). i18n files now use `_()`: menu.c, ipc_commands.c, main.c, config.c, toolbar.c, keyboard_actions.c.

### ~~5.3 Empty docs/user/ and docs/dev/ directories~~ DONE
**Effort**: S | **Category**: Documentation

Resolved in sprint 5. Directories do not exist in the repository (git does not track empty directories). No references found in codebase. Documentation is comprehensive with 5 man pages, QUICKSTART.md, ARCHITECTURE.md, FLUXBOX-COMPAT.md, CONTRIBUTING.md, and DEBRIEF.md — no placeholder directories needed.

### ~~5.4 CONTRIBUTING.md improvements~~ DONE
**Effort**: S | **Category**: Developer Experience

Fixed in sprint 2 (commit `a57c3c9`). Added `--check-config` documentation, fuzz target guide, and WLCS section to CONTRIBUTING.md.

### ~~5.5 Packaging improvements~~ DONE
**Effort**: M | **Category**: Packaging

Verified in sprint 2. Flatpak sha256 was fixed in 1.0.0 release. Nix flake version updated to 1.0.0. Desktop entries enhanced with Categories, Keywords, GenericName (commit `aaf8b21`). Packaging build verification deferred to CI/CD setup (2.1).

---

## 5.6 SECURITY.md — DONE

Created `SECURITY.md` (commit `99419de`) with vulnerability reporting process, security measures inventory, and audit history. Linked from CONTRIBUTING.md.

---

## 6. Feature Roadmap

*Items in this section are deferred — they require dedicated development sprints.*

### 6.1 Workspace slide transitions
**Effort**: L | **Category**: Feature | **Status**: DONE (Sprint 6)

Implemented horizontal slide animation between workspaces using timer-based animation in `src/workspace.c`. Configurable via `session.screen0.workspaceTransition` (`none`, `fade`, `slide`) and `session.screen0.workspaceTransitionDuration` (ms). Uses ease-in-out cubic easing at ~60fps.

### 6.2 ext-image-capture-source / ext-image-copy-capture protocols
**Effort**: L | **Category**: Feature/Protocol

These are the modern replacements for `wlr-screencopy`. Currently Fluxland supports `wlr-screencopy` but not the ext- protocol equivalents being standardized.

**Action**: Implement when wlroots 0.19 provides helpers, or implement directly against the protocol XML.

### 6.3 Color management protocol
**Effort**: XL | **Category**: Feature/Protocol

No color management support (`ext-color-management-v1`). Increasingly important for HDR displays and color-accurate workflows.

### 6.4 Native dockapp protocol for Slit
**Effort**: L | **Category**: Feature | **Status**: DONE (Sprint 6)

Implemented native Wayland dockapp support via window rules. XDG toplevels matching `[Dock] {yes}` in the apps file are routed to the slit during map. Added `wm_slit_add_native_client()` API and `wm_slit_client_type` enum to `src/slit.c/h`.

### 6.5 Conditional keybindings (full support)
**Effort**: M | **Category**: Feature | **Status**: DONE (Sprint 6)

Implemented per-binding conditions using `{condition}` syntax before `:Action` in keybind definitions. Supported conditions: `Maximized`, `Fullscreen`, `Shaded`, `Focused`. Conditions are evaluated at dispatch time in `src/keyboard.c` via `wm_keybind_check_condition()`.

### 6.6 Per-workspace wallpaper (native)
**Effort**: M | **Category**: Feature | **Status**: DONE (Sprint 6)

Implemented native wallpaper rendering in `src/wallpaper.c/h`. Supports PNG loading via Cairo with four display modes (stretch, center, tile, fill). Per-workspace wallpaper paths configurable via `session.screen0.workspaceN.wallpaper`. Scene buffer nodes under `layer_background` with automatic switching on workspace change.

---

## 7. Long-Term / Aspirational

### 7.1 Plugin/extension system
**Effort**: XL | **Category**: Architecture

Allow users to extend the compositor with plugins (Lua, shared libraries, or IPC-based extensions). Would enable community-contributed features without core changes.

### 7.2 Wayland Conformance Test Suite (WLCS) integration
**Effort**: L | **Category**: Testing

The build system already has a `wlcs` option and `src/wlcs_shim.c` exists. Full WLCS integration would validate protocol compliance and catch edge cases in XDG shell, layer shell, and other protocol implementations.

### 7.3 Screen recording / pipewire integration
**Effort**: L | **Category**: Feature

PipeWire-based screen capture for screen recording and streaming (e.g., for OBS, xdg-desktop-portal integration).

### 7.4 Touch and gesture support improvements
**Effort**: M | **Category**: Feature

While pointer-gestures-v1 is initialized, full touch gesture support (pinch-to-zoom, three-finger swipe for workspace switching) would improve tablet/touchscreen usability.

### 7.5 Accessibility improvements
**Effort**: L | **Category**: Accessibility

The IPC has accessibility meta-subscription support, but deeper accessibility integration (screen reader support via AT-SPI, keyboard-only navigation for all UI elements, high-contrast theme improvements) would broaden the user base.

### 7.6 Performance profiling and optimization
**Effort**: M | **Category**: Performance

No known performance issues, but systematic profiling of:
- Decoration rendering hot path (called on every frame with damage)
- Menu rendering with many items
- IPC command response time under load
- Scene graph operations with many windows (50+ views)

---

## Appendix: Effort Estimates

| # | Item | Effort | Priority | Category | Status |
|---|------|--------|----------|----------|--------|
| 1.1 | Missing NULL checks after malloc | S | Critical | Security | **DONE** |
| 1.2 | Missing wlroots scene node checks | S | Critical | Robustness | **DONE** |
| 1.3 | XWayland XKB keymap failure | M | Critical | Bug | **RESOLVED** (env) |
| 1.4 | ShowDesktop double-press | S | Medium | Bug | **DONE** |
| 2.1 | CI/CD pipeline | M | High | Infrastructure | Open |
| 2.2 | Consolidate double-fork helper | S | High | Code Quality | **DONE** |
| 2.3 | Decompose mega-functions | L | High | Code Quality | **DONE** |
| 2.4 | Environment variable sanitization | S | High | Security | **DONE** |
| 3.1 | IPC sprintf → snprintf | S | Medium | Security | **DONE** |
| 3.2 | Consistent config parser error handling | M | Medium | Robustness | **DONE** |
| 3.3 | Split menu.c | L | Medium | Code Quality | **DONE** |
| 3.4 | Split view.c and cursor.c | M each | Medium | Code Quality | **DONE** |
| 4.1 | Test files for untested modules | XL | High | Testing | **DONE** |
| 4.2 | Expand fuzz targets | M | High | Testing | **DONE** |
| 4.3 | XWayland test coverage | L | Medium | Testing | **DONE** |
| 4.4 | Integration test edge cases | M | Medium | Testing | **DONE** |
| 5.1 | Missing man pages (init, startup, ctl) | M | High | Docs | **DONE** |
| 5.2 | i18n infrastructure completion | M | Medium | i18n | **DONE** |
| 5.3 | Empty docs directories | S | Low | Docs | Deferred |
| 5.4 | CONTRIBUTING.md improvements | S | Low | DX | **DONE** |
| 5.5 | Packaging improvements | M | Medium | Packaging | **DONE** |
| 5.6 | SECURITY.md | S | High | Docs | **DONE** |
| 6.1 | Workspace slide transitions | L | Medium | Feature | DONE |
| 6.2 | ext-image-capture protocols | L | Medium | Feature | Deferred |
| 6.3 | Color management protocol | XL | Low | Feature | Deferred |
| 6.4 | Native dockapp protocol | L | Low | Feature | DONE |
| 6.5 | Full conditional keybindings | M | Low | Feature | DONE |
| 6.6 | Native per-workspace wallpaper | M | Low | Feature | DONE |
| 7.1 | Plugin/extension system | XL | Low | Architecture | Deferred |
| 7.2 | WLCS integration | L | Medium | Testing | Deferred |
| 7.3 | PipeWire screen recording | L | Low | Feature | Deferred |
| 7.4 | Touch/gesture improvements | M | Low | Feature | Deferred |
| 7.5 | Accessibility improvements | L | Medium | Accessibility | Deferred |
| 7.6 | Performance profiling | M | Low | Performance | Deferred |

**Effort key**: S = < 2 hours | M = 2–8 hours | L = 1–3 days | XL = 1+ week

### Recommended Execution Order

1. ~~**Sprint 1 — Quick wins**: Items 1.1, 1.2, 1.4, 2.2, 2.4, 3.1~~ **DONE**
2. ~~**Sprint 2 — Medium/Low priority**: Items 1.3, 3.2, 3.3, 4.3, 4.4, 5.1, 5.2, 5.4, 5.5, 5.6~~ **DONE**
3. ~~**Sprint 3 — Code quality**: Item 2.3~~ **DONE** — decomposed 4 mega-functions
4. ~~**Sprint 4 — Test coverage**: Items 4.1, 4.2~~ **DONE** — 11 test files (157 tests), 3 fuzz targets
5. ~~**Sprint 5 — Code quality**: Items 3.4, 5.3~~ **DONE** — split view.c and cursor.c, resolved empty docs dirs
6. **Next up**: Item 2.1 (CI/CD) — unlocks automated quality gates
7. **Features** (ongoing): Items from sections 6–7 based on user demand
